#!/usr/bin/env bash
# End-to-end tests against a running tiny-http binary. Needs curl and python3.
set -euo pipefail

BIN=${1:-./tiny-http}
PORT=$(( 20000 + RANDOM % 20000 ))
URL="http://127.0.0.1:$PORT"
LOG=$(mktemp)
fail=0

"$BIN" --port "$PORT" --threads 4 --read-timeout-ms 1000 > "$LOG" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true; rm -f "$LOG"' EXIT

for _ in $(seq 50); do
	curl -s -o /dev/null "$URL/" && break
	sleep 0.1
done

check() { # name expected actual
	if [ "$2" = "$3" ]; then
		printf '  ok   %s\n' "$1"
	else
		printf '  FAIL %s: expected %s, got %s\n' "$1" "$2" "$3"
		fail=1
	fi
}
code() { curl -s -o /dev/null -w '%{http_code}' -m 5 "$@"; }

echo "integration: $BIN on port $PORT"
check "GET / is 200"                 200 "$(code "$URL/")"
check "GET /hello is 200"            200 "$(code "$URL/hello")"
check "query string ignored in match" 200 "$(code "$URL/hello?name=plant")"
check "trailing slash normalised"    200 "$(code "$URL/hello/")"
check "unknown path is 404"          404 "$(code "$URL/nope")"
check "wrong method is 405"          405 "$(code -X DELETE "$URL/hello")"
check "405 carries Allow"            "Allow: GET" "$(curl -s -i -m 5 -X DELETE "$URL/hello" | tr -d '\r' | grep '^Allow:')"
check "handler reads query"          "Hello, plant!" "$(curl -s -m 5 "$URL/hello?name=plant")"
check "response has Content-Length"  "Content-Length: 14" "$(curl -s -i -m 5 "$URL/hello?name=plant" | tr -d '\r' | grep '^Content-Length:')"

body=$(curl -s -m 5 -X POST --data-binary 'hello world' -H 'Content-Type: text/plain' "$URL/echo")
check "POST body echoed"      "yes" "$(grep -q 'body_len=11' <<<"$body" && tail -1 <<<"$body" | grep -q 'hello world' && echo yes || echo no)"
check "headers echoed"        "yes" "$(grep -qi 'header Content-Type: text/plain' <<<"$body" && echo yes || echo no)"

check "10 KB header is 431"   431 "$(code -H "X-Big: $(head -c 10000 /dev/zero | tr '\0' a)" "$URL/")"
check "2 MB body is 413"      413 "$(head -c 2000000 /dev/zero | code -X POST --data-binary @- "$URL/echo")"
check "chunked body is 501"   501 "$(code -X POST -H 'Transfer-Encoding: chunked' --data-binary 'x' "$URL/echo")"
check "Expect: 100-continue"  200 "$(head -c 5000 /dev/zero | tr '\0' b | code -X POST -H 'Expect: 100-continue' --data-binary @- "$URL/echo")"

# Request split across two writes must still parse.
split=$(python3 - "$PORT" <<'PY'
import socket, sys, time
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(b"GET /hel")
time.sleep(0.2)
s.sendall(b"lo HTTP/1.1\r\nHost: x\r\n\r\n")
print(s.recv(4096).split(b"\r\n")[0].decode())
PY
)
check "split request parses"  "HTTP/1.1 200 OK" "$split"

# An idle connection is answered with 408 and dropped after the read timeout.
idle=$(python3 - "$PORT" <<'PY'
import socket, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.settimeout(3)
print(s.recv(4096).split(b"\r\n")[0].decode())
PY
)
check "idle connection gets 408" "HTTP/1.1 408 Request Timeout" "$idle"

# Slowloris: many idle connections must not starve real ones (4 workers, 1 s timeout).
python3 - "$PORT" <<'PY' &
import socket, sys, time
socks = [socket.create_connection(("127.0.0.1", int(sys.argv[1]))) for _ in range(30)]
time.sleep(4)
PY
SLOW=$!
sleep 0.5
check "served while 30 idle conns held" 200 "$(code -m 8 "$URL/")"
wait "$SLOW" || true

# Graceful stop on SIGTERM: exit 0 and a stopped line in the log.
kill -TERM "$PID"
wait "$PID"; rc=$?
check "SIGTERM exits 0"          0 "$rc"
check "log records the stop"     "yes" "$(grep -q 'tiny-http stopped' "$LOG" && echo yes || echo no)"
trap 'rm -f "$LOG"' EXIT

if [ "$fail" -ne 0 ]; then
	echo "integration: FAILED"; echo "--- server log"; cat "$LOG"; exit 1
fi
echo "integration: all passed"
