---
name: webserv-development
description: Help finish, debug, test, and explain this repository's 42 Webserv HTTP server in C++98, using its assignment requirements and existing architecture. Use for work on this webserv project and preparation for its evaluation.
---

# Webserv development

Help the user complete this work in progress while understanding the code well enough to explain and modify it during evaluation. Implement requested changes in the existing project and explain the relevant design decisions, data flow, and failure handling in plain language.

## Locate the project and its requirements

This skill lives at `skills/webserv-development/SKILL.md`; repository paths below are relative to the project root, two directories above the skill folder. Do not depend on a particular user's absolute filesystem path.

Read relevant source files and the current Git diff before changing behavior. Before every code addition or modification, locate and read the project's `docs/webserv_subject.pdf` and ensure the proposed change complies with its requirements. Do not rely only on memory or previous reads. If `docs/webserv_subject.pdf` is missing, unreadable, or conflicts with another assignment document, ask Arina and wait for clarification before changing code. This PDF is the project's assignment authority (version 24.1 when this skill was created).

`readME.md` records the team's division of work. It is not yet the required submission `README.md`. The file named `test` contains historical manual commands and a networking trace, not an executable automated test suite.

## Preserve the architecture

Arina owns only the request-handling/business-logic responsibilities described in the current `readME.md`. Read that file before choosing which code to modify; the table below is a navigation aid, not an ownership authority. If a proposed change affects another teammate's part or shared integration code, explain the issue and proposed change to Arina, ask for approval, and wait for it before editing. You may inspect those files to understand the problem while approval is pending.

| Layer | Main files | Responsibility |
| --- | --- | --- |
| Networking — Anush | `src/NetworkManager.cpp`, `src/ServerSocket.cpp`, `src/Client.cpp`, corresponding headers | Socket lifecycle, readiness, buffering, connections, multiple listeners, timeouts |
| HTTP and configuration — Milena | `src/HttpParser.cpp`, `src/HttpRequest.cpp`, `src/HttpResponse.cpp`, `src/ResponseBuilder.cpp`, `src/ConfigParser.cpp`, `src/Config.cpp`, `src/ServerConfig.cpp`, `src/Location.cpp` | Parse configuration and requests; represent and serialize HTTP messages |
| Request handling — Arina | `src/RequestHandler.cpp`, `include/HttpTypes.hpp`, `include/RequestHandler.hpp` | Location selection, methods, files, uploads, listing, redirects, CGI behavior |
| Integration | `main.cpp` | Connect parsing, body limits, handler results, configured error pages, and serialization to the network callback |

Keep HTTP parsing out of the networking classes and socket operations out of the parser and business handler. CGI needs coordinated integration with the event loop; do not hide blocking subprocess I/O inside the handler.

Two request/response representations currently coexist: `HttpRequest` / `HttpResponse` classes and `HTTPRequest` / `HTTPResponse` structures. `main.cpp::processRequest` adapts between them. Check both ends when changing fields or headers rather than assuming they are interchangeable.

`NetworkManager` owns client descriptor lifetimes. `Client` objects are stored by value; closing descriptors in their destructor would allow copied or temporary objects to close live connections. Preserve explicit ownership and cleanup when extending these classes.

## Assignment constraints

- Build with the existing Makefile: `c++`, `-Wall -Wextra -Werror -std=c++98`. Do not introduce C++11 features, Boost, or external implementation libraries. Check the subject's permitted functions when adding low-level operations.
- Keep a single readiness loop for all client/server I/O, including listening sockets. Sockets and other descriptors that can wait for data, including CGI pipes, must be nonblocking and read or written only after readiness notification. Regular disk files are exempt.
- Do not inspect `errno` to adjust behavior after read/write operations, including `recv`/`send`. Avoid the usual read-until-`EAGAIN` pattern when it violates this assignment's readiness rules.
- Preserve partial request buffering and partial response writes. Bound request lifetime and clean up disconnected clients, descriptors, and CGI children.
- Use `fork` only for CGI. CGI must not stall other clients; integrate pipe readiness and process timeouts with the server.
- Finish mandatory features before bonuses. Host-based virtual hosting is optional in this subject; serving different content through multiple configured ports is mandatory.

## Work toward completion

For a broad request to finish the project, compare live behavior and source against the mandatory requirements, then work through concrete gaps. Distinguish implemented code, verified behavior, and unfinished or unverified features. Keep the scope aligned with the user's request rather than replacing the project with another server or framework.

The following is a source snapshot from 2026-10-04, not a permanent task list. Reinspect before acting:

- Parsing, networking, GET/POST/DELETE handling, location matching, redirects, autoindex, error-page loading, and body-size checks have implementations; they still require behavior validation.
- CGI extension/interpreter directives are parsed, but `RequestHandler` currently dispatches directly to GET/POST/DELETE without executing CGI. Implement and verify at least one CGI type, including request environment/query arguments, decoded chunked bodies, stdin EOF, output headers/body, correct working directory, timeout, and child cleanup.
- The sample configuration references `www/files`, `www/errors`, and `www/cgi-bin`; these directories were absent in the inspected snapshot. Provide demonstration fixtures when working on those features and check whether error-page fallback works without configured files.
- Uploads currently write a raw request body to a URI-derived filename. Verify the required browser upload workflow before deciding whether multipart handling is needed; do not assume configuration support proves upload completeness.
- Path safety currently uses substring checks. When changing file handling, verify encoded traversal, root containment, symlinks, and safe output escaping in directory listings.
- The command-line entrypoint currently requires one configuration argument. The subject permits an explicit argument or a default path; do not label default-path support mandatory by itself.
- Submission requires an English root `README.md` with the prescribed italicized curriculum line, Description, Instructions, and Resources including how AI was used. Obtain actual contributor logins rather than inventing them.

## Validate the changed behavior

Build with `make`. For build-system work, also check that a second `make` does not unnecessarily relink. Start the server from the repository root because sample paths are relative:

```sh
./webserv configs/webserv.conf
```

Use the configured interface and port; the sample uses `127.0.0.1:8080`. A basic read-only smoke check is:

```sh
curl -i --max-time 5 http://127.0.0.1:8080/
```

Choose checks that exercise the change: split requests across TCP writes, malformed framing, chunked bodies, size limits, method restrictions, redirects, directory behavior, uploads/deletes, simultaneous clients, slow/disconnecting clients, or multiple listeners. Use disposable fixtures for upload/delete checks so existing `www/uploads` content is preserved. Stop only server processes started for the current validation.

For CGI, test a successful script and a stalled/failing script while another client receives a static file. For networking, use a raw-socket test or a syscall trace when needed to verify readiness and partial I/O behavior. Before declaring the mandatory project complete, demonstrate its configured features in a browser and run concurrency/stress checks. Compare specific uncertain HTTP behavior with NGINX or authoritative protocol documentation when needed.

Report what changed, what was actually tested, and remaining gaps. Do not describe source inspection or historical traces as passing runtime tests. Explain enough for the user to defend the implementation, including why the chosen approach satisfies the subject.
