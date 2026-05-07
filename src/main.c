#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include "../include/protocol.h"

int shm_id = -1;
int msg_id = -1; // Message Queue ID
shared_state_t *shared_data = NULL;

pid_t child_pids[MAX_NODES];
int num_children = 0;

void cleanup_and_exit(int sig) {
    printf("\n[Dispatcher] Caught signal %d. Shutting down system...\n", sig);
    
    for (int i = 0; i < num_children; i++) {
        kill(child_pids[i], SIGTERM);
    }
    while (wait(NULL) > 0); 

    if (shared_data != NULL) shmdt(shared_data); 
    if (shm_id != -1) shmctl(shm_id, IPC_RMID, NULL); 
    
    // Destroy the Message Queue
    if (msg_id != -1) {
        msgctl(msg_id, IPC_RMID, NULL);
        printf("[Dispatcher] Message Queue destroyed.\n");
    }
    
    printf("[Dispatcher] All nodes terminated. Exiting safely.\n");
    exit(0);
}

// Message Queue Receiver
void intersection_routine(int node_id) {
    // Re-fetch the message queue ID in the child process
    key_t msg_key = ftok(".", MSG_QUEUE_PROJECT_ID);
    int local_msg_id = msgget(msg_key, 0666);
    
    printf("[Node %d] Booted. Listening on Msg Queue...\n", node_id);
    
    vehicle_msg_t incoming_car;
    // SysV message types must be > 0. We map Node 0 -> Type 1
    long my_msg_type = node_id + 1; 
    
    // The size passed to msgrcv MUST exclude the 'long' type variable
    size_t msg_size = sizeof(vehicle_msg_t) - sizeof(long);

    while(1) {
        // msgrcv blocks until a message with my_msg_type arrives
        if (msgrcv(local_msg_id, &incoming_car, msg_size, my_msg_type, 0) != -1) {
            printf("[Node %d] Received Vehicle %d! (Destination: Node %d)\n", 
                   node_id, incoming_car.vehicle_id, incoming_car.destination_node);
        }
    }
}

int main() {
    signal(SIGINT, cleanup_and_exit);
    srand(time(NULL)); // Initialize random seed for vehicle generation

    // Shared Memory Init
    key_t shm_key = ftok(".", SHM_PROJECT_ID);
    shm_id = shmget(shm_key, sizeof(shared_state_t), IPC_CREAT | 0666);
    shared_data = (shared_state_t *) shmat(shm_id, NULL, 0);

    for(int i=0; i<MAX_NODES; i++) {
        for(int j=0; j<MAX_NODES; j++) {
            shared_data->edge_weights[i][j] = -1; 
        }
    }
    shared_data->active_vehicles = 0;

    // Message Queue Init
    key_t msg_key = ftok(".", MSG_QUEUE_PROJECT_ID);
    if (msg_key == -1) { perror("msg ftok failed"); exit(1); }
    
    msg_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_id == -1) { perror("msgget failed"); cleanup_and_exit(SIGINT); }
    printf("[Dispatcher] Message Queue initialized.\n");

    // Parse Config
    FILE *file = fopen("config/network.txt", "r");
    if (!file) { perror("Failed to open config"); cleanup_and_exit(SIGINT); }

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
    printf("[Dispatcher] Network loaded. Total Intersections: %d\n", max_node_id + 1);

    // Spawn Processes
    for (int i = 0; i <= max_node_id; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("Fork failed"); cleanup_and_exit(SIGINT); }
        
        if (pid == 0) {
            signal(SIGINT, SIG_IGN); 
            signal(SIGTERM, SIG_DFL);
            intersection_routine(i);
            exit(0); 
        } else {
            child_pids[num_children++] = pid;
        }
    }

    printf("[Dispatcher] System Online. Commencing Traffic Injection...\n");
    
    // Message Queue Sender
    int vehicle_counter = 1000;
    size_t msg_size = sizeof(vehicle_msg_t) - sizeof(long);

    while(1) { 
        sleep(3); // Spawn a car every 3 seconds

        int start_node = rand() % (max_node_id + 1);
        int dest_node = rand() % (max_node_id + 1);
        while (dest_node == start_node) dest_node = rand() % (max_node_id + 1); // Avoid 0-distance trips

        vehicle_msg_t new_car;
        new_car.message_type = start_node + 1; // Direct the message to the start node
        new_car.vehicle_id = vehicle_counter++;
        new_car.destination_node = dest_node;

        if (msgsnd(msg_id, &new_car, msg_size, 0) == -1) {
            perror("[Dispatcher] Failed to spawn vehicle");
        } else {
            printf("\n[Dispatcher] --> Spawning Vehicle %d at Node %d (Dest: %d)\n", 
                   new_car.vehicle_id, start_node, dest_node);
        }
    }

    return 0;
}