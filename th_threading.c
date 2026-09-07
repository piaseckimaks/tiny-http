#include "th_threading.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "th_handle_connection.h"

typedef struct th_node {
	int fd;
	struct th_node* next;
} th_node_t;

struct th_pool {
	pthread_t* threads;
	int count;
	pthread_mutex_t mu;
	pthread_cond_t cv;
	th_node_t* head;
	th_node_t* tail;
	int stopping;
	th_server_t* server;
};

static void* worker(void* arg){
	th_pool_t* pool = arg;
	for(;;){
		pthread_mutex_lock(&pool->mu);
		while(!pool->stopping && pool->head == NULL){
			pthread_cond_wait(&pool->cv, &pool->mu);
		}
		if(pool->head == NULL){ /* stopping and drained */
			pthread_mutex_unlock(&pool->mu);
			return NULL;
		}
		th_node_t* node = pool->head;
		pool->head = node->next;
		if(pool->head == NULL){
			pool->tail = NULL;
		}
		pthread_mutex_unlock(&pool->mu);

		int fd = node->fd;
		free(node);
		th_handle_connection(pool->server, fd);
	}
}

th_pool_t* th_pool_create(int threads, th_server_t* server){
	th_pool_t* pool = calloc(1, sizeof *pool);
	if(pool == NULL){
		return NULL;
	}
	pool->threads = calloc((size_t)threads, sizeof(pthread_t));
	if(pool->threads == NULL){
		free(pool);
		return NULL;
	}
	pool->server = server;
	pthread_mutex_init(&pool->mu, NULL);
	pthread_cond_init(&pool->cv, NULL);

	for(int i = 0; i < threads; i++){
		if(pthread_create(&pool->threads[i], NULL, worker, pool) != 0){
			th_pool_stop(pool); /* joins the ones that started */
			return NULL;
		}
		pool->count++;
	}
	return pool;
}

int th_pool_submit(th_pool_t* pool, int fd){
	th_node_t* node = malloc(sizeof *node);
	if(node == NULL){
		return -1;
	}
	node->fd = fd;
	node->next = NULL;

	pthread_mutex_lock(&pool->mu);
	if(pool->stopping){
		pthread_mutex_unlock(&pool->mu);
		free(node);
		return -1;
	}
	if(pool->tail == NULL){
		pool->head = node;
	} else {
		pool->tail->next = node;
	}
	pool->tail = node;
	pthread_cond_signal(&pool->cv);
	pthread_mutex_unlock(&pool->mu);
	return 0;
}

void th_pool_stop(th_pool_t* pool){
	pthread_mutex_lock(&pool->mu);
	pool->stopping = 1;
	pthread_cond_broadcast(&pool->cv);
	pthread_mutex_unlock(&pool->mu);

	for(int i = 0; i < pool->count; i++){
		pthread_join(pool->threads[i], NULL);
	}
	/* Workers drain the queue before exiting, so nothing is left here
	 * unless no worker ever started. Close whatever remains. */
	for(th_node_t* n = pool->head; n != NULL;){
		th_node_t* next = n->next;
		close(n->fd);
		free(n);
		n = next;
	}
	pthread_cond_destroy(&pool->cv);
	pthread_mutex_destroy(&pool->mu);
	free(pool->threads);
	free(pool);
}
