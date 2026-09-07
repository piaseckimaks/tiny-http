#include "header_list.h"
#include <stdlib.h>

void add_header(hl_node_t** head, header_t* header){
    hl_node_t *temp, *p;

    temp = malloc(sizeof(hl_node_t));
    temp->header = header;
    temp->next = NULL;

    if(*head == NULL){
        *head = temp;
        return;
    }

    p = *head;
    while(p->next != NULL){
        p = p->next;
    }
    p->next = temp;
}

void clean_hl_mem(hl_node_t* head){
    if(head == NULL){
        return;
	}

	hl_node_t* temp;
	temp = head;
	head = head->next;
	free(temp->header);
	free(temp);
	clean_hl_mem(head);
}
