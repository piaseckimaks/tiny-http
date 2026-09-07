#ifndef TH_HANDLE_CONNECTION_H
#define TH_HANDLE_CONNECTION_H

#include "tiny_http.h"

/* Serves exactly one request on fd and closes it. Runs on a worker. */
void th_handle_connection(th_server_t* server, int fd);

#endif
