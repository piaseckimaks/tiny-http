#ifndef HEADER_LIST_H
#define HEADER_LIST_H

typedef struct {
    char* key;
	char* value;
} header_t;

typedef struct hl_node{
    header_t* header;
	struct hl_node* next;
} hl_node_t;

void clean_hl_mem(hl_node_t* head);
void add_header(hl_node_t** head, header_t* header);
#endif
