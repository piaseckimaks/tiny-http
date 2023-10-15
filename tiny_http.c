#include <stdint.h>
#include <uchar.h>
#define _GNU_SOURCEtin
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> 
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include "tiny_http.h"
#include "console_utils.h"
#include "th_file_utils.h"
#include "th_threading.h"

#define PORT 8000
#define MAX_EVENTS 10

tiny_http_t server;
/*
 * It creates file descriptor for TCP socket and call th_create_epoll to create epoll event loop.
 */
void th_create_server( const char* ip, int port){
    server.ip_addr = inet_addr(ip);
	server.tcp_port = htons(port);  

  th_create_threads();

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
  	printf("Type stop to gracefully terminate the server\n");
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
  
 	while(1){
		int nfds;
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
     
      if(events_container[n].data.fd == server.tcpfd){
				int connection_socket = 0;
				error_check(connection_socket = accept(server.tcpfd, NULL, NULL), "accept");
						 
				if(connection_socket > 0){
					int* ptr_connection_socket = malloc(sizeof(int));
					*ptr_connection_socket = connection_socket;
					th_delegate_work(ptr_connection_socket);	
				}
			}
		}
	}

	close(server.tcpfd);
   
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
