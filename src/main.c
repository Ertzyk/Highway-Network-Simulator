#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "../include/protocol.h"

int shm_id = -1;
int msg_id = -1; 
shared_state_t *shared_data = NULL;

pid_t child_pids[MAX_NODES];
int num_children = 0;
int total_network_nodes = 0;
int my_node_id = -1;

extern int get_next_hop(shared_state_t *state, int current_node, int dest_node, int total_nodes);

#define NUM_WORKER_THREADS 3
#define LOCAL_QUEUE_SIZE 100

typedef struct {
    vehicle_msg_t cars[LOCAL_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;       
    pthread_cond_t data_ready;  
    int node_id;
    int local_msg_id;
} thread_pool_t;

thread_pool_t local_pool;

void cleanup_and_exit(int sig) {
    printf("\n[Dyspozytor] Przechwycono sygnał %d. Rozpoczęto zamykanie systemu...\n", sig);
    
    for (int i = 0; i < num_children; i++) {
        kill(child_pids[i], SIGTERM);
    }
    while (wait(NULL) > 0); 
    
    if (shared_data != NULL) {
        sem_destroy(&shared_data->global_lock);
        shmdt(shared_data); 
    }

    if (shm_id != -1) shmctl(shm_id, IPC_RMID, NULL); 
    
    if (msg_id != -1) {
        msgctl(msg_id, IPC_RMID, NULL);
        printf("[Dyspozytor] Kolejka komunikatów zniszczona.\n");
    }
    
    printf("[Dyspozytor] Wszystkie węzły wyłączone. Bezpieczne wyjście.\n");
    exit(0);
}

void handle_traffic_jam(int sig) {
    (void)sig;
    
    if (my_node_id == -1 || shared_data == NULL) return;

    int target_neighbor = -1;
    for(int i = 0; i < total_network_nodes; i++) {
        if(shared_data->edge_weights[my_node_id][i] != -1) {
            target_neighbor = i;
            if (rand() % 2 == 0) break; 
        }
    }

    if (target_neighbor != -1) {
        sem_wait(&shared_data->global_lock);
        
        shared_data->edge_weights[my_node_id][target_neighbor] += 1000;
        shared_data->edge_weights[target_neighbor][my_node_id] += 1000;
        
        printf("\n\033[1;31m [SYGNAŁ] WYPADEK W WĘŹLE %d! Droga do węzła %d jest zablokowana! Czas przejazdu wzrósł o 1000!\033[0m\n\n", 
               my_node_id, target_neighbor);
               
        sem_post(&shared_data->global_lock);
    }
}

void* worker_thread_logic(void* arg) {
    int thread_id = *((int*)arg);
    free(arg); 
    
    while(1) {
        vehicle_msg_t car_to_process;
        
        pthread_mutex_lock(&local_pool.lock);
        
        while(local_pool.count == 0) {
            pthread_cond_wait(&local_pool.data_ready, &local_pool.lock);
        }
        
        car_to_process = local_pool.cars[local_pool.head];
        local_pool.head = (local_pool.head + 1) % LOCAL_QUEUE_SIZE;
        local_pool.count--;
        
        pthread_mutex_unlock(&local_pool.lock);
        
        if (car_to_process.destination_node == local_pool.node_id) {
            printf("[Węzeł %d | Wątek %d] Pojazd %d DOTARŁ DO CELU!\n", 
                   local_pool.node_id, thread_id, car_to_process.vehicle_id);
            continue; 
        }

        int next_node = get_next_hop(shared_data, local_pool.node_id, car_to_process.destination_node, total_network_nodes);
        
        if (next_node == -1) {
            printf("[Węzeł %d | Wątek %d] Pojazd %d utknął. Brak trasy do %d.\n", 
                   local_pool.node_id, thread_id, car_to_process.vehicle_id, car_to_process.destination_node);
            continue;
        }

        printf("[Węzeł %d | Wątek %d] Pojazd %d rusza do Węzła %d (Cel: %d)\n", 
               local_pool.node_id, thread_id, car_to_process.vehicle_id, next_node, car_to_process.destination_node);

        sem_wait(&shared_data->global_lock);
        shared_data->active_vehicles++;
        sem_post(&shared_data->global_lock);

        sleep(1); 

        car_to_process.message_type = next_node + 1;
        size_t msg_size = sizeof(vehicle_msg_t) - sizeof(long);
        msgsnd(local_pool.local_msg_id, &car_to_process, msg_size, 0);
    }
    return NULL;
}

void intersection_routine(int node_id) {
    my_node_id = node_id; 
    
    signal(SIGUSR1, handle_traffic_jam);
    
    key_t msg_key = ftok(".", MSG_QUEUE_PROJECT_ID);
    local_pool.local_msg_id = msgget(msg_key, 0666);
    local_pool.node_id = node_id;
    local_pool.head = 0;
    local_pool.tail = 0;
    local_pool.count = 0;
    
    pthread_mutex_init(&local_pool.lock, NULL);
    pthread_cond_init(&local_pool.data_ready, NULL);
    
    pthread_t threads[NUM_WORKER_THREADS];
    for(int i = 0; i < NUM_WORKER_THREADS; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread_logic, thread_id);
    }
    
    printf("[Węzeł %d] Uruchomiono. Utworzono %d wątków roboczych. Nasłuchuję...\n", node_id, NUM_WORKER_THREADS);
    
    vehicle_msg_t incoming_car;
    long my_msg_type = node_id + 1; 
    size_t msg_size = sizeof(vehicle_msg_t) - sizeof(long);

    while(1) {
        if (msgrcv(local_pool.local_msg_id, &incoming_car, msg_size, my_msg_type, 0) != -1) {
            
            pthread_mutex_lock(&local_pool.lock);
            
            if (local_pool.count < LOCAL_QUEUE_SIZE) {
                local_pool.cars[local_pool.tail] = incoming_car;
                local_pool.tail = (local_pool.tail + 1) % LOCAL_QUEUE_SIZE;
                local_pool.count++;
                
                pthread_cond_signal(&local_pool.data_ready);
            } else {
                printf("[Węzeł %d] ⚠️ Przepełnienie kolejki! Pojazd %d odrzucony.\n", node_id, incoming_car.vehicle_id);
            }
            
            pthread_mutex_unlock(&local_pool.lock);
        }
    }
}

int main() {
    signal(SIGINT, cleanup_and_exit);
    srand(time(NULL)); 

    key_t shm_key = ftok(".", SHM_PROJECT_ID);
    shm_id = shmget(shm_key, sizeof(shared_state_t), IPC_CREAT | 0666);
    shared_data = (shared_state_t *) shmat(shm_id, NULL, 0);

    for(int i=0; i<MAX_NODES; i++) {
        for(int j=0; j<MAX_NODES; j++) {
            shared_data->edge_weights[i][j] = -1; 
        }
    }
    shared_data->active_vehicles = 0;
    
    if (sem_init(&shared_data->global_lock, 1, 1) != 0) {
        perror("Błąd inicjalizacji semafora");
        cleanup_and_exit(SIGINT);
    }

    key_t msg_key = ftok(".", MSG_QUEUE_PROJECT_ID);
    if (msg_key == -1) { perror("Błąd ftok dla msg"); exit(1); }
    
    msg_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_id == -1) { perror("Błąd msgget"); cleanup_and_exit(SIGINT); }

    FILE *file = fopen("config/network.txt", "r");
    if (!file) { perror("Brak pliku konfiguracyjnego"); cleanup_and_exit(SIGINT); }

    char line[256];
    int max_node_id = -1; 

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int u, v, weight;
        if (sscanf(line, "%d %d %d", &u, &v, &weight) == 3) {
            shared_data->edge_weights[u][v] = weight;
            shared_data->edge_weights[v][u] = weight;
            if (u > max_node_id) max_node_id = u;
            if (v > max_node_id) max_node_id = v;
        }
    }
    fclose(file);
    total_network_nodes = max_node_id + 1;

    for (int i = 0; i <= max_node_id; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("Błąd fork"); cleanup_and_exit(SIGINT); }
        
        if (pid == 0) {
            signal(SIGINT, SIG_IGN); 
            signal(SIGTERM, SIG_DFL);
            intersection_routine(i);
            exit(0); 
        } else {
            child_pids[num_children++] = pid;
        }
    }

    printf("[Dyspozytor] System Online. Rozpoczynam iniekcję ruchu...\n");
    
    int vehicle_counter = 1000;
    size_t msg_size = sizeof(vehicle_msg_t) - sizeof(long);

    while(1) { 
        sleep(2); 

        if (rand() % 100 < 15) {
            int unlucky_node = rand() % total_network_nodes;
            kill(child_pids[unlucky_node], SIGUSR1);
        }

        int start_node = rand() % total_network_nodes;
        int dest_node = rand() % total_network_nodes;
        while (dest_node == start_node) dest_node = rand() % total_network_nodes;

        vehicle_msg_t new_car;
        new_car.message_type = start_node + 1; 
        new_car.vehicle_id = vehicle_counter++;
        new_car.destination_node = dest_node;

        if (msgsnd(msg_id, &new_car, msg_size, 0) != -1) {
            printf("\n[Dyspozytor] --> Tworzenie Pojazdu %d w Węźle %d (Cel: %d)\n", 
                   new_car.vehicle_id, start_node, dest_node);
        }
    }

    return 0;
}