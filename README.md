# tiny-http

A small HTTP/1.1 server for Linux written in C with no dependencies beyond
libc and pthreads. It exists to learn how the layer under a web framework
works: sockets, epoll, a thread pool, request framing, and routing.

## Build and run

```sh
make            # builds ./tiny-http with -Wall -Wextra -Werror
make test       # unit tests + end-to-end tests (needs curl and python3)
./tiny-http --port 8080
```

Options: `--bind IP` (default 0.0.0.0), `--port N` (8080), `--threads N` (8),
`--read-timeout-ms N` (5000). Stop with Ctrl-C or SIGTERM; in-flight
requests finish first.

## Using it as a library

```c
#include "tiny_http.h"

static void hello(const th_request_t* req, th_response_t* res){
    th_response_printf(res, "Hello from %s %s\n", req->method, req->path);
}

int main(void){
    th_server_t* s = th_server_create("0.0.0.0", 8080, NULL);
    th_server_add_route(s, "GET", "/hello", hello);
    int rc = th_server_listen(s);   /* blocks until SIGINT/SIGTERM */
    th_server_destroy(s);
    return rc;
}
```

Handlers run on a worker thread and get a parsed request: method, path
(query removed, trailing slash stripped), query string, version, headers
via `th_request_header`, and the body with its length. They fill a response
with `th_response_set_status`, `th_response_set_header`, and
`th_response_write` or `th_response_printf`. The server adds
`Content-Length`, `Connection: close`, and a default `Content-Type`.

Pass a `th_config_t` from `th_config_default()` to change thread count,
timeouts, and size limits.

## How it works

- One thread runs an epoll loop over the listening socket and a self-pipe.
  Accepted sockets get `SO_RCVTIMEO` and `SO_SNDTIMEO` and go into a FIFO.
- A fixed pool of workers takes sockets from the FIFO, reads one request,
  matches a route, writes one response, and closes the socket.
- The request reader loops on `recv` until the blank line that ends the
  head, then reads exactly `Content-Length` bytes. It answers
  `Expect: 100-continue`.
- SIGINT and SIGTERM write to the self-pipe. The loop stops accepting,
  workers drain the queue, and `th_server_listen` returns.

## Behaviour a client will see

| Situation | Response |
|---|---|
| Unknown path | 404 |
| Known path, wrong method | 405 with `Allow` |
| Head larger than `max_header_bytes` | 431 |
| Body larger than `max_body_bytes` | 413 |
| `Transfer-Encoding: chunked` | 501 |
| Malformed request line or header | 400 |
| No bytes within `read_timeout_ms` | 408 |

## Limits

One request per connection, no keep-alive. No TLS: put it behind a reverse
proxy. No chunked request bodies. No static file serving. The FIFO between
the acceptor and the workers is unbounded.

## Layout

| File | Role |
|---|---|
| `tiny_http.[ch]` | public API, epoll loop, signals, shutdown |
| `th_request.[ch]` | reading and parsing a request |
| `th_response.[ch]` | building and sending a response |
| `th_threading.[ch]` | worker pool and FIFO |
| `th_handle_connection.[ch]` | one connection: read, route, reply |
| `main.c` | example program with three routes |
| `tests/` | `test_request.c` unit tests, `integration.sh` end-to-end |
