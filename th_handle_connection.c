#include "th_handle_connection.h"

th_route_t* routes = NULL;
int routes_count = 0;

/*
* Still under construction
*/
void* th_handle_connection(void* ptr_connection_socket){
  int connection_socket = *((int*)ptr_connection_socket);

	char buffer[1024];
	recv(connection_socket, buffer, 1024, 0);

	printf("%s", buffer);
	//usleep(500000);
	//system("clear");
  //request_t* request = malloc(sizeof(request_t));
	//request_string_to_struct(buffer, request);
  char* method = strtok(buffer, " ");
	char* route = strtok(NULL, " ");
	
	printf("Checking route %s with method %s\n", route, method);
	
	const char* response = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n <h1>404 Not found</h1>";
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
