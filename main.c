/* Example program: three routes on top of the tiny-http library.
 *
 *   ./tiny-http [--bind IP] [--port N] [--threads N] [--read-timeout-ms N]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiny_http.h"

static void handle_root(const th_request_t* req, th_response_t* res){
	(void)req;
	th_response_set_header(res, "Content-Type", "text/html; charset=utf-8");
	th_response_write_str(res, "<h1>tiny-http</h1><p>It works.</p>\n");
}

/* GET /hello?name=plant -> "Hello, plant!" */
static void handle_hello(const th_request_t* req, th_response_t* res){
	const char* name = "world";
	if(strncmp(req->query, "name=", 5) == 0 && req->query[5] != '\0'){
		name = req->query + 5;
	}
	th_response_printf(res, "Hello, %s!\n", name);
}

/* Any method on /echo: reflects the parsed request back as text. */
static void handle_echo(const th_request_t* req, th_response_t* res){
	th_response_printf(res, "method=%s\npath=%s\nquery=%s\nversion=%s\n",
	                   req->method, req->path, req->query, req->version);
	for(size_t i = 0; i < req->header_count; i++){
		th_response_printf(res, "header %s: %s\n", req->headers[i].name, req->headers[i].value);
	}
	th_response_printf(res, "body_len=%zu\n\n", req->body_len);
	if(req->body_len > 0){
		th_response_write(res, req->body, req->body_len);
	}
}

static int usage(const char* argv0){
	fprintf(stderr, "usage: %s [--bind IP] [--port N] [--threads N] [--read-timeout-ms N]\n", argv0);
	return 2;
}

int main(int argc, char** argv){
	const char* bind_ip = "0.0.0.0";
	int port = 8080;
	th_config_t cfg = th_config_default();

	for(int i = 1; i < argc; i++){
		const char* a = argv[i];
		const char* v = (i + 1 < argc) ? argv[i + 1] : NULL;
		if(strcmp(a, "--bind") == 0 && v){
			bind_ip = v;
			i++;
		} else if(strcmp(a, "--port") == 0 && v){
			port = atoi(v);
			i++;
		} else if(strcmp(a, "--threads") == 0 && v){
			cfg.threads = atoi(v);
			i++;
		} else if(strcmp(a, "--read-timeout-ms") == 0 && v){
			cfg.read_timeout_ms = atoi(v);
			i++;
		} else {
			return usage(argv[0]);
		}
	}

	setvbuf(stdout, NULL, _IOLBF, 0);

	th_server_t* server = th_server_create(bind_ip, port, &cfg);
	if(server == NULL){
		perror("th_server_create");
		return 1;
	}
	th_server_add_route(server, "GET", "/", handle_root);
	th_server_add_route(server, "GET", "/hello", handle_hello);
	th_server_add_route(server, "GET", "/echo", handle_echo);
	th_server_add_route(server, "POST", "/echo", handle_echo);

	int rc = th_server_listen(server);
	if(rc != 0){
		perror("th_server_listen");
	}
	th_server_destroy(server);
	return rc == 0 ? 0 : 1;
}
