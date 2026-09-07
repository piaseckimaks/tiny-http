#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* pipe2, accept4, memmem */
#endif
#include "tiny_http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "th_internal.h"

th_config_t th_config_default(void){
	th_config_t c = {
		.threads = 8,
		.read_timeout_ms = 5000,
		.write_timeout_ms = 10000,
		.max_header_bytes = 8192,
		.max_body_bytes = 1u << 20,
		.backlog = 128,
		.log_requests = 1,
	};
	return c;
}

th_server_t* th_server_create(const char* ip, int port, const th_config_t* cfg){
	if(port < 1 || port > 65535){
		errno = EINVAL;
		return NULL;
	}
	th_server_t* s = calloc(1, sizeof *s);
	if(s == NULL){
		return NULL;
	}
	s->cfg = cfg ? *cfg : th_config_default();
	if(s->cfg.threads < 1){
		s->cfg.threads = 1;
	}
	s->listen_fd = -1;
	s->epoll_fd = -1;
	s->addr.sin_family = AF_INET;
	s->addr.sin_port = htons((uint16_t)port);
	if(inet_pton(AF_INET, ip, &s->addr.sin_addr) != 1){
		free(s);
		errno = EINVAL;
		return NULL;
	}
	if(pipe2(s->wake_fd, O_CLOEXEC | O_NONBLOCK) != 0){
		free(s);
		return NULL;
	}
	/* A peer that resets mid-reply must not kill the process. */
	signal(SIGPIPE, SIG_IGN);
	return s;
}

int th_server_add_route(th_server_t* s, const char* method, const char* path, th_handler_t handler){
	if(s->running || handler == NULL || *method == '\0' || *path != '/'){
		errno = EINVAL;
		return -1;
	}
	th_route_t* grown = realloc(s->routes, (s->route_count + 1) * sizeof *grown);
	if(grown == NULL){
		return -1;
	}
	s->routes = grown;
	th_route_t* r = &s->routes[s->route_count];
	r->method = strdup(method);
	r->path = strdup(path);
	r->handler = handler;
	if(r->method == NULL || r->path == NULL){
		free(r->method);
		free(r->path);
		return -1;
	}
	/* Normalise like the parser does so "/x/" and "/x" are one route. */
	size_t plen = strlen(r->path);
	while(plen > 1 && r->path[plen - 1] == '/'){
		r->path[--plen] = '\0';
	}
	s->route_count++;
	return 0;
}

void th_server_stop(th_server_t* s){
	char b = 1;
	ssize_t r = write(s->wake_fd[1], &b, 1);
	(void)r;
}

/* Signal handling: the handler only writes to the self-pipe. */
static volatile sig_atomic_t g_wake_fd = -1;

static void on_signal(int sig){
	(void)sig;
	int fd = g_wake_fd;
	if(fd >= 0){
		char b = 1;
		ssize_t r = write(fd, &b, 1);
		(void)r;
	}
}

static void set_timeout(int fd, int opt, int ms){
	struct timeval tv = { .tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000 };
	setsockopt(fd, SOL_SOCKET, opt, &tv, sizeof tv);
}

static int epoll_add(int epfd, int fd){
	struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int open_listener(th_server_t* s){
	int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if(fd < 0){
		return -1;
	}
	int one = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	if(bind(fd, (struct sockaddr*)&s->addr, sizeof s->addr) != 0 ||
	   listen(fd, s->cfg.backlog) != 0){
		int saved = errno;
		close(fd);
		errno = saved;
		return -1;
	}
	s->listen_fd = fd;
	return 0;
}

static void accept_one(th_server_t* s){
	int fd = accept4(s->listen_fd, NULL, NULL, SOCK_CLOEXEC);
	if(fd < 0){
		switch(errno){
			case EAGAIN:
			case EINTR:
			case ECONNABORTED:
				return; /* routine, nothing to do */
			case EMFILE:
			case ENFILE:
				fprintf(stderr, "tiny-http: accept: %s\n", strerror(errno));
				usleep(10000); /* the listener stays readable; avoid a hot spin */
				return;
			default:
				fprintf(stderr, "tiny-http: accept: %s\n", strerror(errno));
				return;
		}
	}
	set_timeout(fd, SO_RCVTIMEO, s->cfg.read_timeout_ms);
	set_timeout(fd, SO_SNDTIMEO, s->cfg.write_timeout_ms);
	if(th_pool_submit(s->pool, fd) != 0){
		close(fd);
	}
}

int th_server_listen(th_server_t* s){
	if(s->running){
		errno = EBUSY;
		return -1;
	}
	if(open_listener(s) != 0){
		return -1;
	}
	s->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if(s->epoll_fd < 0 ||
	   epoll_add(s->epoll_fd, s->listen_fd) != 0 ||
	   epoll_add(s->epoll_fd, s->wake_fd[0]) != 0){
		return -1;
	}
	s->pool = th_pool_create(s->cfg.threads, s);
	if(s->pool == NULL){
		return -1;
	}

	struct sigaction sa, old_int, old_term;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	g_wake_fd = s->wake_fd[1];
	sigaction(SIGINT, &sa, &old_int);
	sigaction(SIGTERM, &sa, &old_term);

	s->running = 1;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &s->addr.sin_addr, ip, sizeof ip);
	fprintf(stdout, "tiny-http listening on %s:%d with %d worker threads\n",
	        ip, ntohs(s->addr.sin_port), s->cfg.threads);

	int rc = 0;
	int stop = 0;
	struct epoll_event events[16];
	while(!stop){
		int n = epoll_wait(s->epoll_fd, events, 16, -1);
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			rc = -1;
			break;
		}
		for(int i = 0; i < n; i++){
			int fd = events[i].data.fd;
			if(fd == s->wake_fd[0]){
				char drain[64];
				while(read(s->wake_fd[0], drain, sizeof drain) > 0){
				}
				stop = 1;
			} else if(fd == s->listen_fd){
				accept_one(s);
			}
		}
	}

	/* Stop accepting first, then let workers finish what is queued. */
	fprintf(stdout, "tiny-http stopping, draining %d worker threads\n", s->cfg.threads);
	close(s->listen_fd);
	s->listen_fd = -1;
	th_pool_stop(s->pool);
	s->pool = NULL;

	sigaction(SIGINT, &old_int, NULL);
	sigaction(SIGTERM, &old_term, NULL);
	g_wake_fd = -1;
	close(s->epoll_fd);
	s->epoll_fd = -1;
	s->running = 0;
	fprintf(stdout, "tiny-http stopped\n");
	return rc;
}

void th_server_destroy(th_server_t* s){
	if(s == NULL){
		return;
	}
	for(size_t i = 0; i < s->route_count; i++){
		free(s->routes[i].method);
		free(s->routes[i].path);
	}
	free(s->routes);
	if(s->listen_fd >= 0){
		close(s->listen_fd);
	}
	if(s->epoll_fd >= 0){
		close(s->epoll_fd);
	}
	close(s->wake_fd[0]);
	close(s->wake_fd[1]);
	free(s);
}
