# Arina's application layer

Changes are limited to the handler, application components, demonstration files,
and application tests. Networking, HTTP/configuration parsing, response formatting,
and shared integration code are unchanged.

## Implemented application behaviour

- Longest matching route, including locations ending in a slash.
- GET files, directory redirects, index files, escaped autoindex and encoded links.
- URI percent decoding once, with malformed escapes/control bytes rejected.
- Parent traversal segments rejected; filenames such as `..notes.txt` remain valid.
- GET/POST/DELETE method checks and configured redirects.
- A decoded-body limit check at the handler boundary.
- Raw uploads to a filename, and single-file multipart uploads to the upload route.
- Uploads use exclusive file creation: existing files are refused with 403 rather
  than overwritten. Missing parent directories return 404.
- Configured CGI scripts are not served as source files.

Parent-directory symlink containment and race-resistant filesystem confinement
still need review. Exclusive upload creation prevents following a final symlink,
but does not protect against symlinks in parent directories.

## CGI handoff

CGI is NOT connected to HTTP requests yet. The synchronous `handleRequest()`
returns 501 for configured CGI scripts rather than exposing their source.
The application-side components are ready for integration and testing on POSIX:

1. Supply `CgiContext` with the parser's raw query string, HTTP version, actual
   listener port and remote address. The current shared adapter drops the query.
2. Call `RequestHandler::prepareCgi()`. `CGI_NOT_SELECTED` means use the normal
   handler; `CGI_ERROR` provides an error response; `CGI_READY` provides
   interpreter, script path, environment and decoded request body.
3. Retain one noncopyable `CgiProcess` per CGI request. Call `start()` with the
   prepared values and every existing server/client/CGI descriptor so the child
   closes inherited connections. Interpreter paths must be absolute.
4. Anush adds `inputFd()` with POLLOUT and `outputFd()` with POLLIN to the
   existing SINGLE server poll loop. The CGI component contains no poll call.
5. Dispatch `onWritable()` and `onReadable()` only after the corresponding
   readiness notification. Perform at most one I/O call per notification.
   For stdout POLLIN|POLLHUP, handle POLLIN first and keep draining across
   subsequent notifications. Call `onOutputHangup()` only for HUP without POLLIN.
   Input/output POLLERR or POLLNVAL calls `onPipeError()`.
6. Call `tick(time(NULL))` each loop iteration, even on poll timeout. It uses
   WNOHANG, enforces a five-second deadline and an eight-MiB output cap, and
   reaps terminated children. Keep jobs until `finished()` is true.
7. On disconnect, `cancel()` terminates the job and closes its pipes. If a job
   is destroyed before reaping completes, the component retains its PID:
   call static `CgiProcess::reapAbandoned()` every loop, even without active jobs.
8. Milena parses `output()` as CGI headers and body, including Status and
   Content-Type, and formats the final HTTP response. If `errorStatus()` is
   nonzero, use that status (500 for failures, 504 for timeout), with an error page.

The server must ignore SIGPIPE before pipe writes; its existing NetworkManager
already does. The CGI child restores SIGPIPE's default disposition.
CGI stdin closes after all body bytes are sent; stdout ends at EOF. GET and POST
are supported, and the child changes to the script's directory before execve.
No shell command is used. HTTP_PROXY is not forwarded.

All application headers contain declarations and data members only. Each has a
matching .cpp with its implementation. The Makefile and test runner compile the
five RequestHandler modules, CgiProcess, CgiRequest, CgiContext and MultipartUpload.
It uses C++98 standard-library facilities and the subject-listed POSIX calls.
The subject's external-function list omits file removal despite requiring DELETE:
the existing std::remove usage remains unresolved and should be confirmed with
your evaluator. The child currently terminates through standard C++ std::exit;
confirm whether your evaluator treats C++ C-library wrappers as permitted.

## Demonstration and verification

Build on Linux/macOS with `make`, then start:
`./webserv configs/webserv.conf`. The executable currently requires that argument.

- `/`: static page, a browser upload form and links.
- `/files/`: directory listing and a sample file.
- `/upload/`: uploaded files and the multipart form destination.
- `/old-page`: configured redirect to the new demonstration page.
- `/cgi-bin/hello.py?name=Arina`: GET demo after shared CGI integration.
- POST to the same script: demonstrates CGI stdin after integration.
- `configs/multiple-servers.conf`: includes a second static site on port 9090.

Run `python3 tests/run_application_tests.py` on POSIX. On Windows use
`py -3 tests/run_application_tests.py`. The runner checks the CGI script itself
on either platform, and explicitly skips the C++ component suite when a POSIX
C++98 toolchain is unavailable. Test uploads live only in a temporary directory.

The C++ suite exercises route boundaries, special filenames, traversal and
malformed paths, raw and binary multipart uploads, refusal to overwrite,
body limits, CGI GET/POST, working-directory access, timeout and child failure.
It does not establish browser integration, full-server concurrency, parser
limits, response framing, or symlink safety. Those still need team verification.

## Handler source layout

Every handler .cpp includes its same-name .hpp. Each matching class declares
the methods defined in that .cpp. Shared protected helpers are inherited:
RequestUtils -> RequestRouting -> RequestResources ->
RequestCgi -> RequestHandler.

| Source/header pair | Responsibilities | Functions |
| --- | --- | --- |
| RequestHandler | Request dispatch | 1 |
| RequestRouting | Location selection and filesystem path joining | 5 |
| RequestResources | GET, uploads, DELETE and MIME types | 4 |
| RequestUtils | HTML/URI helpers, traversal checks and error responses | 6 |
| RequestCgi | Method checks and CGI environment preparation | 4 |

The public RequestHandler calls remain handleRequest() and prepareCgi().
CgiResult and its enum values remain accessible through RequestHandler.
No method bodies were changed while introducing matching headers.
The Makefile and application test runner compile all nine application modules.

Additional source/header pairs: CgiProcess, CgiRequest, CgiContext and
MultipartUpload. The C++ test harness also has an application_tests.cpp/.hpp pair.
