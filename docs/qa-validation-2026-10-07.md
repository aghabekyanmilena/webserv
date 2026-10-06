# Webserv validation — 7 October 2026

The initial audit identified five defects, including root escapes through
symlinks. **Those confirmed defects have now been fixed and retested.** Ordinary
requests, concurrency, timeouts, and the exercised allocation-failure paths
remained operational. Browser rendering and optional checklist items still have
the coverage limits described below.

Validation used the [requested checklist](https://www.scribd.com/document/932270026/42-Webserv-QA-Checklist)
and the repository's subject, version 24.1. Tests ran on Linux using Bash, curl,
nc, C++ CGI fixtures, Valgrind, and strace. No Python test runners were used;
Python was exercised as a CGI interpreter. The initial audit did not change
production source; subsequent fixes were authorized separately. Existing
index-related changes were preserved.

## Initial confirmed failures, now fixed

| Priority | Observed result | Relevant implementation |
| --- | --- | --- |
| High | A symlink inside the root exposed an outside fixture with GET **200**. A symlink in the upload directory permitted POST **201** and DELETE **204** against outside files. A symlink to an outside CGI executable returned **200**. | [RequestResources.cpp](../src/RequestResources.cpp), [RequestCgi.cpp](../src/RequestCgi.cpp), [RequestUtils.cpp](../src/RequestUtils.cpp) |
| Medium | With `index hello.py;`, GET `/cgi/` returned **501**, while GET `/cgi/hello.py` returned **200**. Code inspection indicates the same suffix-first dispatch affects compiled CGI directory indices. | [RequestCgi.cpp](../src/RequestCgi.cpp), [RequestResources.cpp](../src/RequestResources.cpp) |
| Medium | `Host: bad host` and a header value containing byte `0x01` were accepted with **200**. The invalid method token `G@T` returned **501** instead of being rejected as malformed. | [HttpParser.cpp](../src/HttpParser.cpp) |
| Medium | Successful DELETE returned **204** with `Content-Length: 0`. That header must be omitted for a 204 response. | [ResponseBuilder.cpp](../src/ResponseBuilder.cpp) |
| Low | `.pdf` returned `application/octet-stream` rather than `application/pdf`. | [RequestResources.cpp](../src/RequestResources.cpp) |

Invalid Host values must receive 400, according to
[RFC 9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2).
The restriction on Content-Length for 204 responses is in
[RFC 9110 §8.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.6).

The root checks currently reject lexical `..` segments but use `stat`, file
streams, and pathname operations that follow symlinks. The upload's exclusive
creation protects the final filename, but an earlier directory component can
still redirect the operation. Retest all four operations together after fixing
containment; checking only the final component is insufficient.

## Initial results by area

This table records the audit before the fixes. Current results are in
**Fix verification** below.

| Area | Result and evidence |
| --- | --- |
| Build | **PASS:** `make re`, C++98, `-Wall -Wextra -Werror`, zero warnings. Subsequent build was up to date. |
| Configuration | **PASS:** missing listen/root, unknown directive, and malformed syntax exited 1. Absolute fixture roots and relative sample roots worked. |
| Index settings | **PASS** for static files: server `index home.html;` was inherited; a location's `index local.html;` overrode it. CGI indices fail as described above. |
| Sample startup | **PASS:** no-argument startup served `/`, `/upload.html`, and the demonstration CGI. `multiple-servers.conf` returned 200 on **8080, 8081, 9090**. |
| Multiple contexts | **PASS:** one server used two ports; two named servers shared a port. Host selection returned distinct content and custom 404 pages; an unmatched Host used the first server. |
| GET and routing | **PASS:** static index, listing, missing file, disabled listing, directory slash redirect, configured redirect, unreadable file, and configured method restrictions. |
| Upload and DELETE | **PASS:** binary POST and multipart upload preserved bytes, duplicate creation returned 403, oversized upload returned 413, DELETE removed the file, and missing DELETE returned 404. Symlink containment and 204 framing fail. |
| Basic malformed requests | **PASS:** missing/duplicate Host, malformed header line, `%00`, above-root traversal, invalid chunk size, and `HTTP/9.9` returned 400. A 9 KB URI returned 431; a 2 MB body returned 413. Additional malformed cases fail as described above. |
| CGI handoff | **PASS:** Python GET/query/POST, shell CGI, and compiled C++ CGI. Method, raw query string, decoded chunked body, content type/length, script name, port, peer address, and HTTP version reached CGI correctly. |
| CGI responses | **PASS:** explicit 201; valid empty 400 preserved with zero body bytes; malformed output and incorrect CGI length returned 500; process failure used the configured 500 page. A 327,680-byte output and simultaneous stdin/stdout transfer completed. |
| CGI lifecycle | **PASS:** static requests remained available during delayed CGI; a hanging CGI returned 504. A disconnected CGI client was cleaned up and its child reaped. |
| Static data | **PASS:** a **12 MiB** file compared byte-for-byte. HTML, CSS, JS, PNG, and JPEG mappings worked. PDF mapping fails. Image rendering requires the browser check below. |
| Response lengths | **PASS:** collected curl responses had Content-Length matching downloaded bytes. The normal 408 declared and delivered **151 bytes**. The separate 204 header prohibition still fails. |
| Concurrent requests | **PASS:** **300/300** simultaneous GETs returned 200. |
| Sustained requests | **PASS:** **9,147 requests** from 50 workers over 30 seconds, all 200. A subsequent 90-second mixed run completed **21,558 requests**, all with their expected 200/404 statuses. Malformed probes during that load still returned 400. |
| Timeout and disconnects | **PASS:** an incomplete request held open for 40 seconds received a complete 408. Partial request/response disconnects were cleaned up; normal requests continued working. |
| Resources | **PASS in this run:** mixed-load RSS samples stayed at **4,052 KiB**; descriptors settled to **5** (three standard streams and two listeners), with no children remaining. These are bounded observations, not proof for arbitrarily long runs. |
| Memory checks | **PASS:** 1,000 GETs under Valgrind plus routing, upload, DELETE, large-file, and CGI checks. **0 errors; 0 bytes in 0 blocks at exit.** No server sockets remained open; Valgrind's own log descriptor was the only extra descriptor reported. SIGINT exit status was 0. |
| Allocation failure | **PASS:** an isolated executable built with `WEBSERV_FAULT_INJECT` returned the prebuilt empty 408 and subsequently served a normal GET with 200. The normal executable was not replaced. This specifically tests throwing from `errorResponse`, not every possible allocation site. |
| I/O model | **PASS for inspected paths:** strace showed accept, client recv/send, and CGI channel recv/send after the corresponding poll readiness. Listeners used `SOCK_NONBLOCK`; server recv/send used `MSG_DONTWAIT`. No `fcntl`, `accept4`, or `pipe2` imports were present. Source inspection found no errno-based decisions after socket reads/writes. |
| Cookies | **PASS for CGI relay:** curl stored Set-Cookie and returned the cookie to CGI via `HTTP_COOKIE`. This does not establish an application session system. |
| README | **PASS:** required italic first line with all three supplied logins, Description/Instructions/Resources, English text, and AI-use disclosure. |

## Checklist differences and remaining coverage

- **Dotfile policy gap:** an in-root `.hidden-fixture` was served with 200.
  The checklist expects denial. The subject does not explicitly require denying
  every dotfile; choose and document a policy separately from outside-root access.
- **Creation-link gap resolved:** POST initially returned 201 without a Location
  header or resource link; uploads now include an encoded Location. A missing
  Location is not by itself invalid when the request URI identifies the resource; see
  [RFC 9110 §15.3.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.3.2).
- The textual `HTTP/2.0` probe returned **400**, whereas the checklist suggests
  **505**. This probe does not test actual HTTP/2 framing. HTTP/1.0 requests worked;
  HTTP/2 implementation is outside this subject's required subset.
- PUT and BREW returned **501**, appropriately for methods not implemented by
  the server. Implemented methods disabled on a route returned **405 with Allow**.
  These distinctions follow [RFC 9110 §9.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.1),
  rather than the checklist's blanket expectation of 405.
- **Browser rendering remains unverified.** The installed Firefox launcher timed
  out after 30 seconds with no screenshot. Retrying its executable directly also
  timed out after 20 seconds and logged a framebuffer mapping failure. Chrome and
  Safari were unavailable.
- IPv6, persistent connections, conditional GET/304, and a persistent session
  demonstration were not established. They are optional or bonus features here.
  CGI types were tested in separate locations with different interpreters.
- macOS `leaks` and Instruments were replaced by Linux Valgrind. No hours-long
  soak test, real system-wide memory exhaustion, or exhaustive fault injection
  was performed. Same-port named contexts were tested; identical duplicate names
  were not separately exercised.
- The root-level `tester` is a **Mach-O x86_64** executable and was not run on
  Linux. It does not constitute a Linux validation result.

## Manual reproduction

Start the supplied server in one terminal:

```bash
make
./webserv configs/multiple-servers.conf
```

In another terminal:

```bash
for port in 8080 8081 9090; do
    curl -i "http://127.0.0.1:$port/"
done
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=QA%2042'
curl -i --data-binary 'hello CGI' http://127.0.0.1:8080/cgi-bin/hello.py
curl -i -X DELETE http://127.0.0.1:8080/                 # 405 + Allow
printf 'GET / HTTP/1.1\r\nHost: bad host\r\n\r\n' |
    nc -w 2 127.0.0.1 8080                              # now 400
seq 300 | xargs -P 300 -n 1 sh -c \
    'curl -s --max-time 10 -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8080/'
```

For the incomplete-request timeout, keep both socket directions open:

```bash
bash -c '
exec 3<>/dev/tcp/127.0.0.1/8080
printf "GET / HTTP/1.1\r\nHost: localhost\r\n" >&3
sleep 40
timeout 3 cat <&3
exec 3>&-
'
curl -i http://127.0.0.1:8080/                          # still 200
```

Open `http://127.0.0.1:8080/upload.html` in your own browser to finish visual
validation: upload a uniquely named file, check the listing, download it, delete
it, and follow the CGI/redirect links. Check browser developer tools for failed
resource requests. Use Ctrl-C to stop the server.

Detailed HTTP captures, safe outside-root fixtures, load results, syscall traces,
and Valgrind logs are in `/tmp/webserv-qa-20261007-4uEZGL`. Temporary Bash harnesses
are `/tmp/webserv-qa-check.sh`, `/tmp/webserv-qa-runtime.sh`, and
`/tmp/webserv-qa-tools.sh`; the checklist harness creates fixtures, while the other
two consume the fixture path saved in `/tmp/webserv-qa-last`. They keep each server
and its checks in the same invocation. These temporary artifacts are not part of
the submitted project.

Two harness expectations were corrected when interpreting results: the initial
empty-error CGI fixture omitted its required Content-Type and correctly returned
500; a valid fixture returned empty 400. Also, the resource comparison initially
required identical descriptor counts and flagged **12 → 5** as a failure; the
decrease reflected completed requests, including the intentionally open timeout
connection. Neither is reported as a project defect. A child-reaping probe taken
after only 0.2 seconds was repeated after the loop had time to reap; no child
remained.

## Fix verification

The five defects above are resolved. Request paths reject symlinks below their
configured root; pinned directory and file descriptors prevent replaced pathnames
from redirecting operations. CGI directory indices execute and retain correct
working-directory access. Invalid method tokens, Host values, header controls,
and bare header newlines receive 400. Responses with status 204 omit both body
and Content-Length. PDF MIME matching also handles uppercase extensions.
Uploads now include an encoded Location pointing to the created resource.

No custom namespaces remain in `src/`, `include/`, or `main.cpp`. Linux file
access requires procfs; symlinks below roots are rejected even if their target is
inside the root. The configured root itself may be a symlink.

Verification after the fixes:

- `make re`: warning-free C++98 build.
- `bash tests/manual_bug_checks.sh`: all checks passed, including inode/parent
  replacement, static and CGI success cases, symlink denials, HTTP validation,
  upload/DELETE headers, and **300/300** concurrent GETs.
- Existing C++ application/CGI and allocation-failure unit checks passed when
  compiled and run directly, without Python runners.
- The broader curl/nc audit passed its applicable assertions, including Python,
  shell, and compiled CGI, multipart/binary data, CGI timeout/disconnect cleanup,
  and 400/413/431/501 responses. Dotfiles and the textual HTTP/2.0 probe retained
  the policies described above rather than the checklist's suggested behavior.
- Supplied ports **8080, 8081, 9090** returned 200. **1,000** mixed static, CGI,
  missing-resource, and malformed-Host requests all returned their expected status.
- Normal and fault-injected incomplete requests returned complete **408**
  responses, with **151** and **0** body bytes respectively. The injected CGI
  failure also returned the prebuilt empty **500**; the server remained available.
- Fresh Valgrind checks covered **1,000 GETs**, CGI indices, uploads, DELETE, and
  denied symlinks: **0 errors, 0 bytes at exit**. Only its own log descriptor
  remained in addition to standard streams. Normal descriptors settled to
  **5**, with no remaining child processes. SIGINT exited successfully.

Fix captures are in `/tmp/webserv-qa-20261007-Zn1g71` and
`/tmp/webserv-bug-checks-4djShf`; the original audit captures remain in the earlier
directory. The reproducible focused checks are available under `tests/`.
No changes have been committed.
