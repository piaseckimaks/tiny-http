#include <pthread.h>
#include "queue.h"
#include "th_handle_connection.h"

void th_delegate_work(void* ptr_connection_socket);
void* th_threads_work(void* arg);
void th_create_threads();
