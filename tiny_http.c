#include <stdint.h>
#include <uchar.h>
#define _GNU_SOURCE
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> 
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "tiny_http.h"
#include "queue.h"
#include "console_utils.h"
#include "th_file_utils.h"

#define PORT 8000
#define MAX_EVENTS 10
#define MAX_THREADS 20

pthread_t thread_pool[MAX_THREADS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_variable = PTHREAD_COND_INITIALIZER;

tiny_http_t server;
th_route_t* routes = NULL;
int routes_count = 0;
/*
 * It creates file descriptor for TCP socket and call th_create_epoll to create epoll event loop.
 */
void th_create_server( const char* ip, int port){
    server.ip_addr = inet_addr(ip);
	server.tcp_port = htons(port);  

	for(int t = 0; t < MAX_THREADS; t++){
	    pthread_create(&thread_pool[t], NULL, delegate_work, NULL);
    }

	server.tcpfd = socket(AF_INET, SOCK_STREAM, 0);


    th_create_epoll();
}

/* 
 * Binds the TCP socket to specified IP address, starts listening and calls th_epoll_event_loop
 */
void th_server_listen(){
	struct sockaddr_in s_addr_in;
	s_addr_in.sin_addr.s_addr = server.ip_addr;
	s_addr_in.sin_family = AF_INET;
	s_addr_in.sin_port = server.tcp_port; 
	error_check(bind(server.tcpfd, (struct sockaddr *) &s_addr_in, sizeof(s_addr_in)), "Failed to bind TCP");
	printf("TCP binded to %s port %i\n", inet_ntoa(s_addr_in.sin_addr), ntohs(server.tcp_port));
	error_check(listen(server.tcpfd, 50), "Listen failed");
  	printf("Server listening for incoming connections\n");
	th_epoll_event_loop();
}
/*
	* Creates epoll file descriptor, adds TCP socket and STDIN file descriptors
*/

void th_create_epoll(){
    error_check((server.epollfd = epoll_create1(0)), "Failed to create epoll file descriptor");
    
    struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = server.tcpfd;

	error_check(epoll_ctl(server.epollfd, EPOLL_CTL_ADD, server.tcpfd, &ev), "epoll_ctl: failed to add TCP file descriptor");

	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;

	error_check(epoll_ctl(server.epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev), "epoll_ctl: failed to add stdin file descriptor");


}
/*
* Starts epoll event loop
*/
void th_epoll_event_loop(){
	struct epoll_event events_container[MAX_EVENTS];
    int connection_socket, nfds;
 	while(1){
		error_check((nfds = epoll_wait(server.epollfd, events_container, MAX_EVENTS, 0)),"epoll_wait");
        
		if(nfds == 0){
			continue;
		}
		
		for(int n = 0; n < nfds; ++n){
            if(events_container[n].data.fd == STDIN_FILENO){
				char* line = NULL;
				size_t linelen = 0;
				int read = getline(&line, &linelen, stdin);
				if(read < 0){
				    perror("Getline: stdin");
			        exit(EXIT_FAILURE);
			    }
				if(strcmp(line, "stop\n") == 0){
					printf("stoping server...\n");
	                close(server.tcpfd);
                    exit(EXIT_SUCCESS);
				}

				printf("Read: %.*s", read, line);
				continue;
			}
			
			#ifdef UDP_ENABLED
			if(events_container[n].data.fd == udp_socket){
				printf("Got data on UDP port\n");
				char buf[512];
                recvfrom(udp_socket, buf, 512, 0, NULL, NULL);
                printf("%s\n", buf);
				continue;
			}
            #endif
			error_check(connection_socket = accept(server.tcpfd, NULL, NULL), "accept");
           
			if(connection_socket > 0){
			    int* ptr_connection_socket = malloc(sizeof(int));
			    *ptr_connection_socket = connection_socket;
				pthread_mutex_lock(&mutex);
			    enqueue(ptr_connection_socket);
				pthread_cond_signal(&condition_variable);
				pthread_mutex_unlock(&mutex);
			}
		}
	}

	close(server.tcpfd);
   
}
/*
* Still under construction
* Currently no checks are done. Reads received buffer and sends 200 OK repsonse
*/
void* handle_connection(void* ptr_connection_socket){
    int connection_socket = *((int*)ptr_connection_socket);

	char buffer[1024];
	recv(connection_socket, buffer, 1024, 0);

	printf("%s", buffer);
	usleep(500000);
	//system("clear");
    //request_t* request = malloc(sizeof(request_t));
	//request_string_to_struct(buffer, request);
  
	const char* response = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n <h1>404 Not found</h1>";
    for(int i = 0; i < routes_count; i++){
        if(!strcmp(routes[i].method, strtok(buffer, " ")) && !strcmp(routes[i].route, strtok(NULL, " "))){
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
* Error check for files descriptor related errors
*/
void error_check(int status, const char* message){
	if(status == -1){
        printf("%s -- errno: %i\n", message, errno);
		printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/*
* TODO: function to gracefully close server when 
* command received from STDIN or in future signal SIGTERM
*/

void gracefully_stopserver(){
	
}

/*
* Loop for the threads to check 
* if there is available work for them in queue
*/
void* delegate_work(void* arg){
	while(1){
        int* ptr_connetion_socket;
        pthread_mutex_lock(&mutex);
		if((ptr_connetion_socket = dequeue()) == NULL){
		    pthread_cond_wait(&condition_variable, &mutex);
			ptr_connetion_socket = dequeue();
		}
		pthread_mutex_unlock(&mutex);
		if (ptr_connetion_socket != NULL) {
            handle_connection(ptr_connetion_socket);
		}
	}
    return NULL;
}

/*
* parsing http request string into struct
* currently using strtok
*/
void request_string_to_struct(char* request_string, request_t* request){
	request->method = strtok(request_string, " ");
	request->route = strtok(NULL, " ");
    request->http_version = strtok(NULL, "\n");
	char* token;
    request->headers_list = NULL;
	hl_node_t** head = &(request->headers_list);
    while(token != NULL){        
        token = strtok(NULL, ":");
		header_t* header = malloc(sizeof(header_t));
		header->key = token;
		token = strtok(NULL, "\n");
		header->value = token;
		add_header(head, header);
	}
    
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
}

// Needed for QUIC ??
#ifdef UDP_ENABLED
void listen_udp(){
    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    printf("UDP socket file descriptor: %i\n", udp_socket);
	struct sockaddr_in s_addr_in_udp;
	s_addr_in_udp.sin_addr.s_addr = INADDR_ANY;
	s_addr_in_udp.sin_family = AF_INET;
	s_addr_in_udp.sin_port = htons(8080); 
	printf("UDP binded to %s port %i\n", inet_ntoa(s_addr_in_udp.sin_addr), 8080);

	error_check(bind(udp_socket, (struct sockaddr*) &s_addr_in_udp, sizeof(s_addr_in_udp)), "Failed to bind UDP");


	ev.events = EPOLLIN;
	ev.data.fd = udp_socket;

	error_check(epoll_ctl(server.epollfd, EPOLL_CTL_ADD, udp_socket, &ev), "epoll_ctl: udp socket");

    
}
#endif
