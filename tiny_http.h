#ifndef TINY_HTTP_H
#define TINY_HTTP_H

#include <stddef.h>

#include "th_request.h"
#include "th_response.h"

/*
 * tiny-http: a small HTTP/1.1 server for Linux. One thread accepts on an
 * epoll loop; a fixed pool of workers reads, routes, and answers. Every
 * connection carries exactly one request and is closed after the reply.
 */

typedef struct th_server th_server_t;

/* Handlers run on a worker thread. The response defaults to 200 with an
 * empty body and text/plain. Set what you need; the server sends it. */
typedef void (*th_handler_t)(const th_request_t* req, th_response_t* res);

typedef struct {
	int threads;             /* worker threads (default 8) */
	int read_timeout_ms;     /* per-connection recv timeout (default 5000) */
	int write_timeout_ms;    /* per-connection send timeout (default 10000) */
	size_t max_header_bytes; /* request head limit, 431 above it (default 8192) */
	size_t max_body_bytes;   /* Content-Length limit, 413 above it (default 1 MiB) */
	int backlog;             /* listen(2) backlog (default 128) */
	int log_requests;        /* one line per request on stdout (default 1) */
} th_config_t;

th_config_t th_config_default(void);

/* Returns NULL with errno set on a bad address or allocation failure. */
th_server_t* th_server_create(const char* ip, int port, const th_config_t* cfg);

/* Routes match method and path exactly, after the query string is removed
 * and any trailing slash stripped. Must be called before th_server_listen.
 * Returns 0 or -1. */
int th_server_add_route(th_server_t* s, const char* method, const char* path, th_handler_t handler);

/* Binds, starts the workers, and serves until th_server_stop is called or
 * SIGINT/SIGTERM arrives. Queued and in-flight requests finish before it
 * returns. Returns 0 on a clean stop, -1 with errno set on failure. */
int th_server_listen(th_server_t* s);

/* Safe to call from any thread or from a signal handler. */
void th_server_stop(th_server_t* s);

void th_server_destroy(th_server_t* s);

#endif
