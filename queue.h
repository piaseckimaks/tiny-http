#ifndef QUEUE_H
#define QUEUE_H

struct node{
    struct node* next;
    int* ptr_connection_socket;
}; 
typedef struct node node_t;

void enqueue(int* ptr_connection_socket);
int* dequeue();

#endif
