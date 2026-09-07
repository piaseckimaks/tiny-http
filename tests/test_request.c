/* Unit tests for the request parser and reader. No server involved. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../th_request.h"

static int failures = 0;
#define CHECK(cond) do { \
	if(!(cond)){ fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while(0)
#define STREQ(a, b) ((a) != NULL && (b) != NULL && strcmp((a), (b)) == 0)

static int parse(th_request_t* req, const char* text){
	th_request_init(req);
	req->buf = strdup(text);
	return th_request_parse_head(req, req->buf, strlen(text));
}

static void test_parse_full(void){
	th_request_t r;
	CHECK(parse(&r, "GET /login?x=1&y=2 HTTP/1.1\r\nHost: a\r\nContent-Type:  text/plain \r\n\r\n") == 0);
	CHECK(STREQ(r.method, "GET"));
	CHECK(STREQ(r.path, "/login"));
	CHECK(STREQ(r.query, "x=1&y=2"));
	CHECK(STREQ(r.version, "HTTP/1.1"));
	CHECK(r.header_count == 2);
	CHECK(STREQ(th_request_header(&r, "host"), "a"));
	CHECK(STREQ(th_request_header(&r, "CONTENT-TYPE"), "text/plain"));
	CHECK(th_request_header(&r, "X-Missing") == NULL);
	th_request_free(&r);
}

static void test_parse_paths(void){
	th_request_t r;
	CHECK(parse(&r, "GET /login/ HTTP/1.1\r\n\r\n") == 0);
	CHECK(STREQ(r.path, "/login"));
	CHECK(STREQ(r.query, ""));
	th_request_free(&r);

	CHECK(parse(&r, "GET / HTTP/1.0\r\n\r\n") == 0);
	CHECK(STREQ(r.path, "/"));
	CHECK(STREQ(r.version, "HTTP/1.0"));
	th_request_free(&r);

	CHECK(parse(&r, "GET /a/b/// HTTP/1.1\r\n\r\n") == 0);
	CHECK(STREQ(r.path, "/a/b"));
	th_request_free(&r);
}

static void test_parse_rejects(void){
	const char* bad[] = {
		"GET\r\n\r\n",
		"GET /x\r\n\r\n",
		"GET x HTTP/1.1\r\n\r\n",
		"GET /x FOO/1.1\r\n\r\n",
		"GET /x HTTP/1.1\r\n",              /* no blank line */
		"GET /x HTTP/1.1\r\nNoColon\r\n\r\n",
		"GET /x HTTP/1.1\r\n: v\r\n\r\n",   /* empty name */
		"GET /x HTTP/1.1\r\nBad Name: v\r\n\r\n",
		" /x HTTP/1.1\r\n\r\n",             /* empty method */
	};
	for(size_t i = 0; i < sizeof bad / sizeof bad[0]; i++){
		th_request_t r;
		if(parse(&r, bad[i]) != -1){
			fprintf(stderr, "FAIL: accepted %s\n", bad[i]);
			failures++;
		}
		th_request_free(&r);
	}
}

/* socketpair lets th_request_read run against bytes we control. */
static th_read_status_t read_from(const char* data, size_t len, size_t max_head, size_t max_body,
                                  int timeout_ms, int close_after, th_request_t* req){
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	if(timeout_ms > 0){
		struct timeval tv = { .tv_sec = 0, .tv_usec = timeout_ms * 1000 };
		setsockopt(sv[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	}
	if(len > 0){
		CHECK(write(sv[1], data, len) == (ssize_t)len);
	}
	if(close_after){
		close(sv[1]);
	}
	th_request_init(req);
	th_read_status_t st = th_request_read(sv[0], req, max_head, max_body);
	close(sv[0]);
	if(!close_after){
		close(sv[1]);
	}
	return st;
}

static void test_read(void){
	th_request_t r;
	const char* get = "GET /hello HTTP/1.1\r\nHost: x\r\n\r\n";
	CHECK(read_from(get, strlen(get), 8192, 1024, 0, 0, &r) == TH_READ_OK);
	CHECK(STREQ(r.path, "/hello"));
	CHECK(r.body_len == 0 && r.body == NULL);
	th_request_free(&r);

	const char* post = "POST /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
	CHECK(read_from(post, strlen(post), 8192, 1024, 0, 0, &r) == TH_READ_OK);
	CHECK(r.body_len == 5 && memcmp(r.body, "hello", 5) == 0);
	th_request_free(&r);

	const char* big = "POST /echo HTTP/1.1\r\nContent-Length: 2048\r\n\r\n";
	CHECK(read_from(big, strlen(big), 8192, 1024, 0, 0, &r) == TH_READ_BODY_TOO_LARGE);
	th_request_free(&r);

	const char* chunked = "POST /echo HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
	CHECK(read_from(chunked, strlen(chunked), 8192, 1024, 0, 0, &r) == TH_READ_UNSUPPORTED);
	th_request_free(&r);

	const char* badlen = "POST /echo HTTP/1.1\r\nContent-Length: abc\r\n\r\n";
	CHECK(read_from(badlen, strlen(badlen), 8192, 1024, 0, 0, &r) == TH_READ_BAD);
	th_request_free(&r);

	char huge[300];
	memset(huge, 'a', sizeof huge);
	memcpy(huge, "GET / HTTP/1.1\r\nX: ", 19);
	CHECK(read_from(huge, sizeof huge, 64, 1024, 0, 0, &r) == TH_READ_HEADERS_TOO_LARGE);
	th_request_free(&r);

	CHECK(read_from("", 0, 8192, 1024, 0, 1, &r) == TH_READ_CLOSED);
	th_request_free(&r);

	const char* partial = "GET /hello HTTP/1.1\r\nHo";
	CHECK(read_from(partial, strlen(partial), 8192, 1024, 0, 1, &r) == TH_READ_BAD);
	th_request_free(&r);

	CHECK(read_from("", 0, 8192, 1024, 50, 0, &r) == TH_READ_TIMEOUT);
	th_request_free(&r);

	/* Body bytes that arrive in the same packet as the head must survive
	 * the parser writing its NUL terminator. */
	const char* joined = "POST /echo HTTP/1.1\r\nContent-Length: 3\r\n\r\nabcEXTRA";
	CHECK(read_from(joined, strlen(joined), 8192, 1024, 0, 0, &r) == TH_READ_OK);
	CHECK(r.body_len == 3 && memcmp(r.body, "abc", 3) == 0);
	th_request_free(&r);
}

int main(void){
	test_parse_full();
	test_parse_paths();
	test_parse_rejects();
	test_read();
	if(failures){
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	printf("test_request: all passed\n");
	return 0;
}
