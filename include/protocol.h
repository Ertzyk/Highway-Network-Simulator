#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <semaphore.h>
#include <sys/msg.h>

// Global Constants
#define MAX_NODES 100
#define SHM_PROJECT_ID 'H' 
#define MSG_QUEUE_PROJECT_ID 'Q'

// Shared Memory Structure
typedef struct {
    int edge_weights[MAX_NODES][MAX_NODES]; 
    int active_vehicles;
    sem_t global_lock; 
} shared_state_t;

// Message Queue Structure
// System V Message Queue structure requires a 'long type' as the first member
typedef struct {
    long message_type;      
    int vehicle_id;
    int destination_node;
} vehicle_msg_t;

#endif