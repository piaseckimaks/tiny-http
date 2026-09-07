#ifndef TH_REQUEST_H
#define TH_REQUEST_H

#include <stddef.h>

#define TH_MAX_HEADERS 64

typedef struct {
	const char* name;
	const char* value;
} th_header_t;

/*
 * A parsed HTTP/1.x request. All string fields point into buffers owned
 * by the struct; call th_request_free when done.
 */
typedef struct {
	const char* method;
	const char* path;    /* target without the query string, trailing slash stripped */
	const char* query;   /* "" when absent */
	const char* version; /* "HTTP/1.0" or "HTTP/1.1" */
	th_header_t headers[TH_MAX_HEADERS];
	size_t header_count;
	const char* body;    /* not NUL-terminated; NULL when body_len == 0 */
	size_t body_len;

	char* buf;      /* owned: raw head bytes, mutated in place by the parser */
	char* body_buf; /* owned */
} th_request_t;

typedef enum {
	TH_READ_OK = 0,
	TH_READ_CLOSED,            /* peer closed before sending anything */
	TH_READ_TIMEOUT,           /* SO_RCVTIMEO expired while waiting for bytes */
	TH_READ_BAD,               /* malformed request */
	TH_READ_HEADERS_TOO_LARGE, /* head exceeded max_header_bytes */
	TH_READ_BODY_TOO_LARGE,    /* Content-Length exceeded max_body_bytes */
	TH_READ_UNSUPPORTED,       /* e.g. Transfer-Encoding: chunked */
	TH_READ_ERROR              /* recv failed; errno is set */
} th_read_status_t;

void th_request_init(th_request_t* req);
void th_request_free(th_request_t* req);

/*
 * Parses a request head held in memory: request line, headers, and the
 * terminating blank line. buf is modified in place and must stay alive
 * as long as req is used. buf[len] must be '\0'. Returns 0 or -1.
 */
int th_request_parse_head(th_request_t* req, char* buf, size_t len);

/* Case-insensitive header lookup. Returns NULL when absent. */
const char* th_request_header(const th_request_t* req, const char* name);

/*
 * Reads one full request from a connected socket: loops on recv until the
 * head is complete, then reads exactly Content-Length body bytes. Relies
 * on SO_RCVTIMEO on fd for the timeout. On any status other than
 * TH_READ_OK the request is partially filled; still call th_request_free.
 */
th_read_status_t th_request_read(int fd, th_request_t* req,
                                 size_t max_header_bytes, size_t max_body_bytes);

#endif
