#include "th_threading.h"
#define MAX_THREADS 20

pthread_t thread_pool[MAX_THREADS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_variable = PTHREAD_COND_INITIALIZER;

void th_delegate_work(void* ptr_connection_socket){
	pthread_mutex_lock(&mutex);
	enqueue(ptr_connection_socket);
	pthread_cond_signal(&condition_variable);
	pthread_mutex_unlock(&mutex);
}
/*
* Loop for the threads to check 
* if there is available work for them in queue
*/
void* th_threads_work(void* arg){
	while(1){
        int* ptr_connetion_socket;
        pthread_mutex_lock(&mutex);
		if((ptr_connetion_socket = dequeue()) == NULL){
		    pthread_cond_wait(&condition_variable, &mutex);
			ptr_connetion_socket = dequeue();
		}
		pthread_mutex_unlock(&mutex);
		if (ptr_connetion_socket != NULL) {
            th_handle_connection(ptr_connetion_socket);
		}
	}
    return NULL;
}

void th_create_threads(){
	for(int t = 0; t < MAX_THREADS; t++){
	    pthread_create(&thread_pool[t], NULL, th_threads_work, NULL);
    }
}
