#include "queue.h"
#include <stdlib.h>

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(int* ptr_connection_socket){
    node_t* newnode = malloc(sizeof(node_t));
	newnode->ptr_connection_socket = ptr_connection_socket;
	newnode->next = NULL;
	
	if (tail == NULL) {
        head = newnode;
	} else {
        tail->next = newnode;
	}
    tail = newnode;
}

int* dequeue(){
    if (head == NULL) {
        return NULL;
	} else {
	    int *result = head->ptr_connection_socket;
		node_t *temp_node = head;
		head = head->next;
		if(head == NULL)
	        tail = NULL;
		free(temp_node);
		return result;
	}
}
