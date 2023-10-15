#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>

typedef struct{
    const char* route;
    const char* method;
	void (*handle_function)();
} th_route_t;


void* th_handle_connection(void* ptr_connection_socket);
void th_add_route(const char* method, const char* path, void (*handler_func)());
