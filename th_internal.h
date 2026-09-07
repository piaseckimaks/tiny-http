/* Shared between the server, the thread pool, and the connection handler.
 * Not part of the public API. */
#ifndef TH_INTERNAL_H
#define TH_INTERNAL_H

#include <netinet/in.h>

#include "tiny_http.h"
#include "th_threading.h"

typedef struct {
	char* method;
	char* path;
	th_handler_t handler;
} th_route_t;

struct th_server {
	th_config_t cfg;
	struct sockaddr_in addr;
	int listen_fd;
	int epoll_fd;
	int wake_fd[2]; /* self-pipe: any write ends the event loop */
	th_route_t* routes;
	size_t route_count;
	int running;
	th_pool_t* pool;
};

#endif
