#ifndef TH_RESPONSE_H
#define TH_RESPONSE_H

#include <stddef.h>

/*
 * A response under construction. Handlers set a status, add headers, and
 * append body bytes; the server serialises and sends it. Content-Length
 * and Connection are always written by the server and cannot be set.
 */
typedef struct {
	int status;
	char* head;      /* owned: "Name: value\r\n" lines */
	size_t head_len;
	size_t head_cap;
	char* body;      /* owned */
	size_t body_len;
	size_t body_cap;
	int has_content_type;
} th_response_t;

void th_response_init(th_response_t* res);
void th_response_free(th_response_t* res);

void th_response_set_status(th_response_t* res, int status);
/* Rejects CR/LF in either argument and the server-managed headers. Returns 0 or -1. */
int th_response_set_header(th_response_t* res, const char* name, const char* value);
int th_response_write(th_response_t* res, const void* data, size_t len);
int th_response_write_str(th_response_t* res, const char* s);
int th_response_printf(th_response_t* res, const char* fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* Serialises and sends the whole response. Returns 0 or -1 with errno set. */
int th_response_send(int fd, const th_response_t* res);

const char* th_status_text(int status);

#endif
