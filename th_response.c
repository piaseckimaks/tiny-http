#include "th_response.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

void th_response_init(th_response_t* res){
	memset(res, 0, sizeof *res);
	res->status = 200;
}

void th_response_free(th_response_t* res){
	free(res->head);
	free(res->body);
	th_response_init(res);
}

void th_response_set_status(th_response_t* res, int status){
	res->status = status;
}

static int reserve(char** buf, size_t* cap, size_t len, size_t extra){
	if(len + extra + 1 <= *cap){
		return 0;
	}
	size_t ncap = *cap ? *cap : 256;
	while(ncap < len + extra + 1){
		ncap *= 2;
	}
	char* nb = realloc(*buf, ncap);
	if(nb == NULL){
		return -1;
	}
	*buf = nb;
	*cap = ncap;
	return 0;
}

static int has_crlf(const char* s){
	return strchr(s, '\r') != NULL || strchr(s, '\n') != NULL;
}

int th_response_set_header(th_response_t* res, const char* name, const char* value){
	if(*name == '\0' || has_crlf(name) || has_crlf(value)){
		return -1;
	}
	if(strcasecmp(name, "Content-Length") == 0 || strcasecmp(name, "Connection") == 0){
		return -1;
	}
	size_t nlen = strlen(name), vlen = strlen(value);
	if(reserve(&res->head, &res->head_cap, res->head_len, nlen + 2 + vlen + 2) != 0){
		return -1;
	}
	int n = snprintf(res->head + res->head_len, res->head_cap - res->head_len,
	                 "%s: %s\r\n", name, value);
	res->head_len += (size_t)n;
	if(strcasecmp(name, "Content-Type") == 0){
		res->has_content_type = 1;
	}
	return 0;
}

int th_response_write(th_response_t* res, const void* data, size_t len){
	if(reserve(&res->body, &res->body_cap, res->body_len, len) != 0){
		return -1;
	}
	memcpy(res->body + res->body_len, data, len);
	res->body_len += len;
	res->body[res->body_len] = '\0';
	return 0;
}

int th_response_write_str(th_response_t* res, const char* s){
	return th_response_write(res, s, strlen(s));
}

int th_response_printf(th_response_t* res, const char* fmt, ...){
	va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	int need = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if(need < 0 || reserve(&res->body, &res->body_cap, res->body_len, (size_t)need) != 0){
		va_end(ap2);
		return -1;
	}
	vsnprintf(res->body + res->body_len, res->body_cap - res->body_len, fmt, ap2);
	va_end(ap2);
	res->body_len += (size_t)need;
	return 0;
}

static int send_all(int fd, const char* p, size_t n){
	while(n > 0){
		ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
		if(w < 0){
			if(errno == EINTR){
				continue;
			}
			return -1;
		}
		p += w;
		n -= (size_t)w;
	}
	return 0;
}

int th_response_send(int fd, const th_response_t* res){
	char status_line[64];
	int sl = snprintf(status_line, sizeof status_line, "HTTP/1.1 %d %s\r\n",
	                  res->status, th_status_text(res->status));
	char fixed[160];
	int fl = snprintf(fixed, sizeof fixed, "Content-Length: %zu\r\nConnection: close\r\n%s\r\n",
	                  res->body_len,
	                  res->has_content_type ? "" : "Content-Type: text/plain; charset=utf-8\r\n");

	size_t total = (size_t)sl + res->head_len + (size_t)fl + res->body_len;
	char* out = malloc(total);
	if(out == NULL){
		errno = ENOMEM;
		return -1;
	}
	char* p = out;
	memcpy(p, status_line, (size_t)sl);      p += sl;
	memcpy(p, res->head, res->head_len);     p += res->head_len;
	memcpy(p, fixed, (size_t)fl);            p += fl;
	memcpy(p, res->body, res->body_len);

	int rc = send_all(fd, out, total);
	free(out);
	return rc;
}

const char* th_status_text(int status){
	switch(status){
		case 100: return "Continue";
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 411: return "Length Required";
		case 413: return "Content Too Large";
		case 414: return "URI Too Long";
		case 422: return "Unprocessable Content";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 503: return "Service Unavailable";
		default:  return "Unknown";
	}
}
