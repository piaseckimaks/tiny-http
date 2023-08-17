#ifndef REQUEST_H
#define REQUEST_H

typedef struct{
    int method;
    char* route;
	float http_version;
	char* body;
}th_request_t;

void th_requeststr_to_request_t(int connection_socket);
void th_clean_requeststr(char* requeststr);

#endif
