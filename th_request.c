#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* memmem */
#endif
#include "th_request.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

void th_request_init(th_request_t* req){
	memset(req, 0, sizeof *req);
	req->query = "";
}

void th_request_free(th_request_t* req){
	free(req->buf);
	free(req->body_buf);
	th_request_init(req);
}

static char* trim(char* s){
	while(*s == ' ' || *s == '\t'){
		s++;
	}
	char* end = s + strlen(s);
	while(end > s && (end[-1] == ' ' || end[-1] == '\t')){
		*--end = '\0';
	}
	return s;
}

int th_request_parse_head(th_request_t* req, char* buf, size_t len){
	if(len == 0 || buf[len] != '\0'){
		return -1;
	}

	/* Request line: METHOD SP target SP HTTP/1.x CRLF */
	char* p = buf;
	char* line_end = strstr(p, "\r\n");
	if(line_end == NULL){
		return -1;
	}
	*line_end = '\0';

	char* sp1 = strchr(p, ' ');
	if(sp1 == NULL){
		return -1;
	}
	*sp1 = '\0';
	char* target = sp1 + 1;
	char* sp2 = strchr(target, ' ');
	if(sp2 == NULL){
		return -1;
	}
	*sp2 = '\0';
	char* version = sp2 + 1;

	if(*p == '\0' || *target != '/' || strncmp(version, "HTTP/1.", 7) != 0 || strlen(version) != 8){
		return -1;
	}
	req->method = p;
	req->version = version;

	char* q = strchr(target, '?');
	if(q != NULL){
		*q = '\0';
		req->query = q + 1;
	} else {
		req->query = "";
	}
	size_t plen = strlen(target);
	while(plen > 1 && target[plen - 1] == '/'){
		target[--plen] = '\0';
	}
	req->path = target;

	/* Headers until the blank line. */
	p = line_end + 2;
	for(;;){
		line_end = strstr(p, "\r\n");
		if(line_end == NULL){
			return -1; /* head must end with a blank line */
		}
		if(line_end == p){
			break;
		}
		*line_end = '\0';

		char* colon = strchr(p, ':');
		if(colon == NULL || colon == p){
			return -1;
		}
		*colon = '\0';
		for(char* c = p; *c; c++){
			if(*c == ' ' || *c == '\t'){
				return -1; /* no whitespace in field names */
			}
		}
		if(req->header_count >= TH_MAX_HEADERS){
			return -1;
		}
		req->headers[req->header_count].name = p;
		req->headers[req->header_count].value = trim(colon + 1);
		req->header_count++;
		p = line_end + 2;
	}
	return 0;
}

const char* th_request_header(const th_request_t* req, const char* name){
	for(size_t i = 0; i < req->header_count; i++){
		if(strcasecmp(req->headers[i].name, name) == 0){
			return req->headers[i].value;
		}
	}
	return NULL;
}

/* Reads into buf until n bytes are present. Returns TH_READ_OK or an error status. */
static th_read_status_t recv_exact(int fd, char* buf, size_t have, size_t want){
	while(have < want){
		ssize_t n = recv(fd, buf + have, want - have, 0);
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			return (errno == EAGAIN || errno == EWOULDBLOCK) ? TH_READ_TIMEOUT : TH_READ_ERROR;
		}
		if(n == 0){
			return TH_READ_BAD;
		}
		have += (size_t)n;
	}
	return TH_READ_OK;
}

th_read_status_t th_request_read(int fd, th_request_t* req,
                                 size_t max_header_bytes, size_t max_body_bytes){
	char* buf = malloc(max_header_bytes + 1);
	if(buf == NULL){
		errno = ENOMEM;
		return TH_READ_ERROR;
	}
	req->buf = buf;

	/* 1. Accumulate until the blank line that ends the head. */
	size_t used = 0;
	size_t head_len = 0;
	for(;;){
		if(used == max_header_bytes){
			return TH_READ_HEADERS_TOO_LARGE;
		}
		ssize_t n = recv(fd, buf + used, max_header_bytes - used, 0);
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			return (errno == EAGAIN || errno == EWOULDBLOCK) ? TH_READ_TIMEOUT : TH_READ_ERROR;
		}
		if(n == 0){
			return used == 0 ? TH_READ_CLOSED : TH_READ_BAD;
		}
		size_t scan_from = used >= 3 ? used - 3 : 0;
		used += (size_t)n;
		char* end = memmem(buf + scan_from, used - scan_from, "\r\n\r\n", 4);
		if(end != NULL){
			head_len = (size_t)(end - buf) + 4;
			break;
		}
	}

	/* 2. Parse the head. The parser needs a NUL at head_len; bytes after it
	 *    are the start of the body, so save and restore that byte. */
	char saved = buf[head_len];
	buf[head_len] = '\0';
	int rc = th_request_parse_head(req, buf, head_len);
	buf[head_len] = saved;
	if(rc != 0){
		return TH_READ_BAD;
	}

	/* 3. Body, framed by Content-Length only. */
	if(th_request_header(req, "Transfer-Encoding") != NULL){
		return TH_READ_UNSUPPORTED;
	}
	size_t content_length = 0;
	const char* cl = th_request_header(req, "Content-Length");
	if(cl != NULL){
		char* endp;
		errno = 0;
		unsigned long long v = strtoull(cl, &endp, 10);
		if(*cl == '\0' || *endp != '\0' || errno != 0 || v > (unsigned long long)SIZE_MAX){
			return TH_READ_BAD;
		}
		content_length = (size_t)v;
	}
	if(content_length > max_body_bytes){
		return TH_READ_BODY_TOO_LARGE;
	}
	if(content_length == 0){
		return TH_READ_OK;
	}

	size_t extra = used - head_len;
	const char* expect = th_request_header(req, "Expect");
	if(extra == 0 && expect != NULL && strcasecmp(expect, "100-continue") == 0){
		static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
		if(send(fd, cont, sizeof cont - 1, MSG_NOSIGNAL) < 0){
			return TH_READ_ERROR;
		}
	}

	char* body = malloc(content_length);
	if(body == NULL){
		errno = ENOMEM;
		return TH_READ_ERROR;
	}
	req->body_buf = body;
	size_t have = extra < content_length ? extra : content_length;
	memcpy(body, buf + head_len, have);
	th_read_status_t st = recv_exact(fd, body, have, content_length);
	if(st != TH_READ_OK){
		return st;
	}
	req->body = body;
	req->body_len = content_length;
	return TH_READ_OK;
}
