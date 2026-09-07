#ifndef TH_THREADING_H
#define TH_THREADING_H

#include "tiny_http.h"

/* A fixed pool of workers fed by an unbounded FIFO of accepted sockets. */
typedef struct th_pool th_pool_t;

th_pool_t* th_pool_create(int threads, th_server_t* server);
int th_pool_submit(th_pool_t* pool, int fd);
/* Lets workers drain the queue, then joins them and frees the pool. */
void th_pool_stop(th_pool_t* pool);

#endif
