#!/usr/bin/env bash
# Linux checks using Bash, curl, nc and a C++98 fixture; no Python test runner.
set -euo pipefail
cd "$(dirname "$0")/.."
port=${WEBSERV_TEST_PORT:-19007}
[[ "$port" =~ ^[0-9]+$ ]] && ((port > 0 && port < 65536))
qa=$(mktemp -d /tmp/webserv-bug-checks-XXXXXX)
pid=
cleanup() {
    if [[ -n "$pid" ]]; then kill -INT "$pid" 2>/dev/null || true; wait "$pid" || true; fi
    printf 'Captures and fixtures: %s\n' "$qa"
}
trap cleanup EXIT
make
mkdir -p "$qa"/{www/list,www/empty,www/upload,www/cgi,outside,native}
c++ -Wall -Wextra -Werror -std=c++98 -Iinclude tests/rooted_path_tests.cpp src/RootedPath.cpp -o "$qa/path-tests"
"$qa/path-tests" "$qa/native"
printf 'home-index' > "$qa/www/home.html"
printf 'inside' > "$qa/www/list/inside.txt"
printf 'outside' > "$qa/outside/file.txt"
printf 'outside-delete' > "$qa/outside/delete.txt"
printf '%%PDF-1.4\n' > "$qa/www/file.PDF"
printf 'relative-cwd' > "$qa/www/cgi/note.txt"
cat > "$qa/www/cgi/index.sh" <<'CGI'
printf 'Content-Type: text/plain\r\n\r\nmethod=%s\nquery=%s\nscript=%s\n' "$REQUEST_METHOD" "$QUERY_STRING" "$SCRIPT_NAME"
cat note.txt
cat
CGI
cp "$qa/www/cgi/index.sh" "$qa/outside/outside.sh"
ln -s "$qa/outside" "$qa/www/escape"
ln -s "$qa/outside" "$qa/www/upload/escape"
ln -s "$qa/outside/file.txt" "$qa/www/final.txt"
ln -s "$qa/outside/outside.sh" "$qa/www/cgi/escape.sh"
ln -s "$qa/www/home.html" "$qa/www/internal.html"
cat > "$qa/config.conf" <<CONF
server {
 host 127.0.0.1; listen $port; root $qa/www; index home.html;
 client_max_body_size 1000000;
 location / { allowed_methods GET; autoindex off; }
 location /list { root $qa/www/list; allowed_methods GET; autoindex on; }
 location /upload { root $qa/www/upload; upload_path $qa/www/upload; allowed_methods GET POST DELETE; autoindex on; }
 location /cgi { root $qa/www/cgi; index index.sh; allowed_methods GET POST; cgi_extension .sh; cgi_path /bin/sh; }
}
CONF
./webserv "$qa/config.conf" > "$qa/server.log" 2>&1 & pid=$!
for i in {1..30}; do
    if curl -s --max-time 1 "http://127.0.0.1:$port/" >/dev/null; then break; fi
    sleep .1
done
kill -0 "$pid"
check() { [[ "$2" == "$3" ]] || { printf 'FAIL %s: expected %s got %s\n' "$1" "$2" "$3"; exit 1; }; printf 'PASS %s\n' "$1"; }
http() {
    local name=$1 expected=$2 path=$3 code
    shift 3
    code=$(curl -sS --max-time 10 --path-as-is -D "$qa/$name.headers" -o "$qa/$name.body" -w '%{http_code}' "$@" "http://127.0.0.1:$port$path")
    check "$name" "$expected" "$code"
}
raw() {
    local name=$1 expected=$2 request=$3
    printf '%b' "$request" | nc -w 2 127.0.0.1 "$port" > "$qa/$name.raw"
    check "$name" "$expected" "$(awk 'NR==1{print $2}' "$qa/$name.raw")"
}
http index 200 /
check inherited-index home-index "$(cat "$qa/index.body")"
http listing 200 /list/
http no-index 403 /empty/
http cgi-index 200 '/cgi/?q=42'
rg -q 'query=q=42' "$qa/cgi-index.body"
rg -q 'script=/cgi/index.sh' "$qa/cgi-index.body"
rg -q 'relative-cwd' "$qa/cgi-index.body"
http cgi-post-index 200 /cgi/ --data-binary 'posted-body'
rg -q 'posted-body' "$qa/cgi-post-index.body"
http cgi-directory-redirect 301 /cgi
for path in /escape/file.txt /final.txt /internal.html /cgi/escape.sh; do
    http "blocked-${path//\//-}" 403 "$path"
done
http upload-escape 403 /upload/escape/new.txt --data-binary outside
test ! -e "$qa/outside/new.txt"
http delete-escape 403 /upload/escape/delete.txt -X DELETE
test -f "$qa/outside/delete.txt"
http upload 201 /upload/new.txt --data-binary 'upload-bytes'
check upload-location /upload/new.txt "$(awk 'tolower($1)=="location:"{gsub("\r", "");print $2}' "$qa/upload.headers")"
http download 200 /upload/new.txt
check uploaded-bytes upload-bytes "$(cat "$qa/download.body")"
http delete 204 /upload/new.txt -X DELETE
! rg -qi '^Content-Length:' "$qa/delete.headers"
test ! -s "$qa/delete.body"
http missing-delete 404 /upload/new.txt -X DELETE
http pdf 200 /file.PDF
check pdf-type application/pdf "$(awk 'tolower($1)=="content-type:"{gsub("\r", "");print $2}' "$qa/pdf.headers")"
raw bad-host 400 'GET / HTTP/1.1\r\nHost: bad host\r\n\r\n'
raw bad-method 400 'G@T / HTTP/1.1\r\nHost: localhost\r\n\r\n'
raw control-header 400 'GET / HTTP/1.1\r\nHost: localhost\r\nX-Bad: a\x01b\r\n\r\n'
raw bare-newline 400 'GET / HTTP/1.1\r\nHost: localhost\nX-Bad: yes\r\n\r\n'
raw unknown-method 501 'BREW / HTTP/1.1\r\nHost: localhost\r\n\r\n'
for host in "localhost:$port" '[::1]' '[2001:db8::1]:8080' '[::ffff:127.0.0.1]' '[v1.name]' 'example%2Ecom'; do
    raw "valid-host-${host//\//-}" 200 "GET / HTTP/1.1\r\nHost: $host\r\n\r\n"
done
for host in 'user@localhost' '[::1' '[example]' '[1::2::3]' 'localhost:abc' 'localhost:12:34'; do
    raw "invalid-host-${host//\//-}" 400 "GET / HTTP/1.1\r\nHost: $host\r\n\r\n"
done
export port
seq 300 | xargs -P 300 -n1 bash -c 'curl -sS --max-time 10 -o /dev/null -w "%{http_code}\n" "http://127.0.0.1:$port/"' > "$qa/concurrent.codes"
check concurrent-300 300 "$(rg -c '^200$' "$qa/concurrent.codes")"
printf 'All bug regression checks passed\n'
