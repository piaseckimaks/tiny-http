CC      ?= gcc
# CFLAGS and LDFLAGS may be overridden on the command line, for example
# for a sanitizer build. The flags the code needs to compile stay separate.
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Werror
LDFLAGS ?=
REQ_CFLAGS  = -D_GNU_SOURCE -pthread -MMD -MP
REQ_LDFLAGS = -pthread

LIB_SRC = tiny_http.c th_request.c th_response.c th_threading.c th_handle_connection.c
LIB_OBJ = $(LIB_SRC:.c=.o)

all: tiny-http

%.o: %.c
	$(CC) $(CFLAGS) $(REQ_CFLAGS) -c -o $@ $<

tiny-http: main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) $(REQ_CFLAGS) -o $@ $^ $(LDFLAGS) $(REQ_LDFLAGS)

tests/test_request: tests/test_request.o th_request.o
	$(CC) $(CFLAGS) $(REQ_CFLAGS) -o $@ $^ $(LDFLAGS) $(REQ_LDFLAGS)

test: tiny-http tests/test_request
	./tests/test_request
	./tests/integration.sh ./tiny-http

clean:
	rm -f *.o *.d tests/*.o tests/*.d tiny-http tests/test_request

-include $(wildcard *.d) $(wildcard tests/*.d)

.PHONY: all test clean
