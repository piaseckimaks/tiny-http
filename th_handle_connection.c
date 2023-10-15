#include "th_handle_connection.h"

th_route_t* routes = NULL;
int routes_count = 0;

/*
* Still under construction
*/
void* th_handle_connection(void* ptr_connection_socket){
	pid_t thread_id = syscall(__NR_gettid);
	printf("==========================\n");
	printf("thread id: %i\n", thread_id);
	printf("==========================\n");
  int connection_socket = *((int*)ptr_connection_socket);
	printf("for connection socket number: %i\n", connection_socket);
	printf("==========================\n");
  const char* response = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n <h1>404 Not found</h1>";

	char buffer[1024];
	int recv_bytes = recv(connection_socket, buffer, 1024, 0);

	//printf("%s", buffer);
	//usleep(300000);
	//system("clear");
  //request_t* request = malloc(sizeof(request_t));
	//request_string_to_struct(buffer, request);
  char* method = strtok(buffer, " ");
	char* route = strtok(NULL, " ");
	
	printf("\nrecv_bytes: %i \n Thread id %i Checking route %s with method %s\n",recv_bytes, thread_id, route, method);

  if(method == NULL || route == NULL){
    printf("Failed to read a route or method from request!\n");
	  send(connection_socket, response, strlen(response), 0);
	  close(connection_socket);
		return NULL;
	}

    for(int i = 0; i < routes_count; i++){
		    
        if(!strcmp(routes[i].method, method) && !strcmp(routes[i].route, route)){
			      routes->handle_function();
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n <h1>Hello</h1>";	
		}
	}
    
    

	send(connection_socket, response, strlen(response), 0);
	//clean_hl_mem(request->headers_list);
	close(connection_socket);
	return NULL;
}
/*
* Adds route to the server
*/ 
void th_add_route(const char* method, const char* path, void (*handler_func)()){
  th_route_t route;
	route.method = method;
	route.route = path;
	route.handle_function = handler_func;
    
	printf("Adding route: %s %s\n", method, path);
	if(routes == NULL){
	  printf("route: %s %s is first\n", method, path);
    routes = malloc(sizeof(th_route_t));
		*routes = route;
		routes_count ++;
	  return;
	}


    routes = realloc(routes, (routes_count + 1) * sizeof(th_route_t));
    routes[routes_count] = route;
		routes_count ++;
}
