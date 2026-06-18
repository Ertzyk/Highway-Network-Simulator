#include <stdio.h>
#include <limits.h>
#include "../include/protocol.h"

__attribute__((visibility("default"))) 
int get_next_hop(shared_state_t *state, int current_node, int dest_node, int total_nodes) {
    if (current_node == dest_node) return current_node;

    int dist[MAX_NODES];
    int parent[MAX_NODES];
    int visited[MAX_NODES] = {0};

    // Initialize arrays
    for(int i = 0; i < total_nodes; i++) { 
        dist[i] = INT_MAX; 
        parent[i] = -1; 
    }
    dist[current_node] = 0;

    // Standard O(V^2) Dijkstra
    for(int count = 0; count < total_nodes - 1; count++) {
        int u = -1, min_dist = INT_MAX;
        
        for(int i = 0; i < total_nodes; i++) {
            if(!visited[i] && dist[i] < min_dist) { 
                min_dist = dist[i]; 
                u = i; 
            }
        }
        
        if(u == -1 || u == dest_node) break;
        visited[u] = 1;

        for(int v = 0; v < total_nodes; v++) {
            if(!visited[v] && state->edge_weights[u][v] != -1 && dist[u] != INT_MAX) {
                int alt = dist[u] + state->edge_weights[u][v];
                if(alt < dist[v]) { 
                    dist[v] = alt; 
                    parent[v] = u; 
                }
            }
        }
    }

    if (parent[dest_node] == -1) return -1; // No path exists
    
    int curr = dest_node;
    while(parent[curr] != current_node) { 
        curr = parent[curr]; 
    }
    
    return curr; // This is the node we need to drive to next
}