#include "th_handle_connection.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "th_internal.h"

static void plain_error(th_response_t* res, int status){
	th_response_set_status(res, status);
	th_response_printf(res, "%d %s\n", status, th_status_text(status));
}

/* Maps a failed read to a status code. Returns 0 when the connection
 * should simply be closed with no reply. */
static int status_for_read(th_read_status_t st){
	switch(st){
		case TH_READ_TIMEOUT:           return 408;
		case TH_READ_BAD:               return 400;
		case TH_READ_HEADERS_TOO_LARGE: return 431;
		case TH_READ_BODY_TOO_LARGE:    return 413;
		case TH_READ_UNSUPPORTED:       return 501;
		case TH_READ_OK:
		case TH_READ_CLOSED:
		case TH_READ_ERROR:
		default:                        return 0;
	}
}

static void route(th_server_t* s, const th_request_t* req, th_response_t* res){
	th_handler_t handler = NULL;
	char allow[256] = "";

	for(size_t i = 0; i < s->route_count; i++){
		const th_route_t* r = &s->routes[i];
		if(strcmp(r->path, req->path) != 0){
			continue;
		}
		if(strcmp(r->method, req->method) == 0){
			handler = r->handler;
			break;
		}
		if(allow[0] != '\0'){
			strncat(allow, ", ", sizeof allow - strlen(allow) - 1);
		}
		strncat(allow, r->method, sizeof allow - strlen(allow) - 1);
	}

	if(handler != NULL){
		handler(req, res);
	} else if(allow[0] != '\0'){
		th_response_set_header(res, "Allow", allow);
		plain_error(res, 405);
	} else {
		plain_error(res, 404);
	}
}

void th_handle_connection(th_server_t* s, int fd){
	th_request_t req;
	th_response_t res;
	th_request_init(&req);
	th_response_init(&res);

	th_read_status_t st = th_request_read(fd, &req, s->cfg.max_header_bytes, s->cfg.max_body_bytes);
	int reply = 1;
	if(st == TH_READ_OK){
		route(s, &req, &res);
	} else {
		int status = status_for_read(st);
		if(status == 0){
			reply = 0;
		} else {
			plain_error(&res, status);
		}
	}

	if(reply){
		th_response_send(fd, &res);
		if(s->cfg.log_requests){
			fprintf(stdout, "%s %s -> %d\n",
			        req.method ? req.method : "-",
			        req.path ? req.path : "-",
			        res.status);
		}
	}

	close(fd);
	th_request_free(&req);
	th_response_free(&res);
}
