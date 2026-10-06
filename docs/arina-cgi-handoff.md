# Arina CGI handoff

Implemented in Arina's application layer:

- `prepareCgi` selects scripts, checks permissions and builds the CGI environment and decoded request body.
- `CgiProcess` runs the interpreter in the script directory, exchanges bytes through nonblocking pipes, waits for EOF and child exit, and kills timed-out scripts.
- `parseCgiOutput` converts completed stdout into `HTTPResponse`: CGI Status, Location redirects, content type, binary body, and Content-Length validation. Missing Content-Length uses the complete EOF-delimited body. Invalid output returns 500. Pipe/process errors remain 500; timeout remains 504.
- Component tests cover GET/POST, working directory, nested POST uploads, large EOF output, malformed CGI headers, hangs and crashes.

## Pending integration

The current NetworkManager has no extra-fd callbacks. Its request processor returns only an immediate response or an incomplete-request flag, so it cannot represent an asynchronous CGI job. No networking, parser, response-builder, or main.cpp files were changed.

The integration needs a client-keyed CGI job, pipe readiness callbacks, an event-loop tick even when poll times out, and cancellation on client removal/shutdown. Keep one poll for all sockets and CGI pipes. Close inherited server/client/other-CGI descriptors in each child.

1. After the HTTP parser completes, call `prepareCgi` with protocol, query, actual listener port and client address. CGI_NOT_SELECTED uses the normal request handler; CGI_ERROR returns the supplied error response.
2. For CGI_READY, start CgiProcess with the plan and the complete list of inherited descriptors. Mark the request consumed and pending; do not start another job on later client reads.
3. Register inputFd for POLLOUT and outputFd for POLLIN. Call onWritable/onReadable only after readiness. For stdout, process POLLIN before POLLHUP so buffered output is retained. HUP without POLLIN calls onOutputHangup. Remove closed descriptors before the next poll.
4. Call tick every event-loop iteration. Once finished, use errorStatus for process errors, otherwise call parseCgiOutput. Serialize through the existing response path and queue the result with sendResponse.
5. On disconnection, unregister descriptors and destroy/cancel the job. Continue reapAbandoned even when no jobs remain.

Do not call a second poll or run CGI synchronously inside handleRequest. The current normal handler intentionally returns 501 for CGI until this integration exists.

## Verification

On a POSIX system with a C++98 compiler:

```sh
make
python3 tests/run_application_tests.py
python3 tests/run_cgi_server_tests.py ./webserv
```

The live-server suite needs the pending integration; it is expected to fail with 501 beforehand. Windows here has no make/POSIX compiler or installed WSL, so C++ compilation and live-server tests cannot be run locally. Python demonstration GET/POST checks passed.

The current HTTPResponse map cannot represent repeated header fields; this parser explicitly rejects duplicates instead of silently discarding them. Repeated Set-Cookie support would require a response-interface change.
