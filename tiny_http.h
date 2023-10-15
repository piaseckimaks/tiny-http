#ifndef TINY_HTTP_H
#define TINY_HTTP_H
#include "header_list.h"
#include <stdint.h>

typedef char BYTE;

typedef struct{
    uint32_t ip_addr;
    uint16_t tcp_port;
	uint16_t udp_port;
    int tcpfd;
	int udpfd;
	int epollfd;
}tiny_http_t;

typedef struct{
    char* method;
	char* route;
	char* http_version;
	hl_node_t* headers_list;
} request_t;

void th_create_server(const char* ip, int port);
void* handle_connection(void* ptr_connection_socket);
void gracefully_stopserver();
void error_check(int status, const char* message);
void request_string_to_struct(char* request_string, request_t* request);
void th_create_epoll();
void th_epoll_event_loop();
void th_server_listen();
void th_add_route(const char* method, const char* path, void (*handler_func)());

#endif
