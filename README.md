*This project has been created as part of the 42 curriculum by atseruny, miaghabe, arimanuk.*

# webserv

## Description

A C++98 HTTP server implementing GET, POST, and DELETE, static files, directory
listing, redirects, uploads, configurable error pages and body limits, and Python
CGI. One `poll()` loop drives listeners, clients, and CGI socket channels. Each response
closes its connection. Incomplete requests time out after thirty seconds, CGI
jobs after five seconds, and queued responses have five seconds to drain.
Prebuilt emergency responses and per-client exception guards contain runtime
allocation failures.

The specification is [docs/webserv_subject.pdf](docs/webserv_subject.pdf).
Linux listeners use `SOCK_NONBLOCK`; client I/O uses `MSG_DONTWAIT`. CGI uses
the subject-listed `socketpair()` function. The server performs poll-driven
`send()` and `recv()` with `MSG_DONTWAIT`, while the child's stdin/stdout remain
blocking. No `fcntl()`, `accept4()`, or `pipe2()` calls are used.

## Instructions

Build on Linux with Make and a C++ compiler, then run from the repository directory:

```sh
make
./webserv
# Alternative configuration: ports 8080, 8081, and 9090
./webserv configs/multiple-servers.conf
```

With no argument, the server loads `configs/webserv.conf` on `127.0.0.1:8080`.
Stop it with Ctrl-C. Sample CGI configurations require `/usr/bin/python3`;
adjust `cgi_path` if your interpreter is elsewhere. Paths in sample configurations
are relative to the repository directory.

`index` can be set in a server block, for example `index home.html;`.
Locations inherit the server's index unless they specify their own `index`.
The default is `index.html`; inheritance works regardless of directive order.
The filename is resolved inside the requested directory under its location root.
CGI directory indices execute through the same process path as explicit script
requests, and receive their effective script URI in `SCRIPT_NAME`.

On Linux, file access uses `O_PATH`, `O_NOFOLLOW`, and `/proc/self/fd` to pin
directories and files before using them. Symlinks below a configured root are
rejected, including links pointing elsewhere inside that root. The configured
root itself may be a symlink. This protection also applies to upload parents,
DELETE, directory indices, and CGI scripts. Linux procfs must be mounted.

Open `http://127.0.0.1:8080/upload.html` for upload, DELETE, and CGI demonstrations.
Other routes include `/files/` (directory listing), `/old-page` (redirect),
`/cgi-bin/hello.py?name=webserv`, and `/missing` (custom 404). Uploads are stored
under `/upload`; existing files cannot be overwritten.

```sh
curl -i http://127.0.0.1:8080/
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=webserv'
curl -i -X POST --data 'hello CGI' http://127.0.0.1:8080/cgi-bin/hello.py
curl -i -X POST --data 'example' http://127.0.0.1:8080/upload/example.txt
curl -i -X DELETE http://127.0.0.1:8080/upload/example.txt
```

`make clean` removes objects and dependencies; `make fclean` also removes the
executable; `make re` rebuilds from scratch. The focused Bash/C++ regression
checks require curl, nc, rg, and a C++98 compiler:

```sh
bash tests/manual_bug_checks.sh
# Choose another unused local port if necessary:
WEBSERV_TEST_PORT=19008 bash tests/manual_bug_checks.sh
```

These checks include symlink rejection, replaced path components, CGI indices,
malformed requests, upload/DELETE, PDF headers, and 300 concurrent GETs. Fixtures
and response captures stay in the printed temporary directory. Existing Python
suites are also available:

```sh
make re
python3 tests/run_application_tests.py
python3 tests/run_cgi_server_tests.py
python3 tests/run_network_unit_tests.py
python3 tests/run_network_tests.py
python3 tests/run_network_tests.py --fault-inject
```

Stop other servers on 8080, 8081, and 9090 before the network suite. Each network
run includes a forty-second half-open request. Fault injection builds a separate
temporary executable with `WEBSERV_FAULT_INJECT`, leaving the normal binary and
flags unchanged. It forces network error-response construction to throw
`std::bad_alloc` and checks the emergency 408 response.

The live CGI suite also requires `curl`. The suites cover multiport serving, 300 concurrent requests, malformed requests,
400/413/431/501 responses, redirects, upload/DELETE, descriptor cleanup, default
configuration, and SIGINT. CGI checks cover query strings, chunk decoding,
relative file access, EOF output, crashes, timeouts, and static requests during
a hanging job. See [docs/integration-review.md](docs/integration-review.md) for
the file-access design and integration review.

## Team ownership

| Contributor | Area | Responsibility |
| --- | --- | --- |
| Anush (`atseruny`) | Networking | Listeners, clients, poll, timeouts, CGI jobs and descriptor cleanup |
| Milena (`miaghabe`) | HTTP and configuration | Request/config parsing, response formatting, status codes and error pages |
| Arina (`arimanuk`) | Application | Routing, file access, uploads, DELETE, CGI preparation and output parsing |

Networking passes raw HTTP bytes to the HTTP/application handoff in `main.cpp`.
The handoff returns a response or a `CgiRequest` plan. `CgiProcess` performs channel
I/O only after networking reports readiness; the application prepares the CGI
environment and parses completed output.

## Resources

- [Project subject](docs/webserv_subject.pdf), version 24.1.
- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110).
- [RFC 9112: HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112).
- [RFC 3875: CGI/1.1](https://www.rfc-editor.org/rfc/rfc3875).
- Linux manual pages: `man 2 socket`, `man 2 accept`, `man 2 poll`, `man 2 recv`,
  `man 2 send`, `man 2 socketpair`, `man 2 fork`, and `man 2 waitpid`.

AI assistance was used in this revision to inspect the subject and code,
implement network exception guards and emergency responses, connect the existing
CGI process to the poll loop, adjust Linux socket handling and the main handoff,
add regression tests, and update documentation and the demonstration page.
It also assisted the symlink-access review and fix, CGI directory-index handoff,
HTTP request validation, response framing, and MIME/creation headers. The existing
CGI process, HTTP/configuration parsers, and application handlers provided the
base for these changes. The team must
review and understand these changes before evaluation. This statement describes
this revision's AI usage, not earlier work.
