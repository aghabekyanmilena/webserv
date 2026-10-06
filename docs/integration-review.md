# Application and CGI integration review

The network integration uses `RequestCgi::prepareCgi()` and
`RequestCgi::parseCgiOutput()`. The HTTP parser's raw query and decoded entity
body reach the plan; chunk framing is removed before stdin is written. CGI
status, headers, and content length use that parser and `ResponseBuilder`.
Valid CGI entity bodies are preserved, including empty error bodies.

Legacy `Cgi.cpp` is excluded from the executable's source list. Its `Cgi.hpp`
declares a different `CgiContext` from `CgiContext.hpp`; linking both definitions
violates the C++ one-definition rule. The reserved legacy source is unchanged
and is not part of the active integration.

## Root escape through symlinks

The application rejects lexical `..` traversal, but `stat()`, `std::ifstream`,
`opendir()`, and interpreter execution follow symbolic links. Consequently:

- GET can serve an outside file through a file or directory symlink.
- CGI preparation can select an outside script through a symlink.
- Uploads use `O_EXCL`, protecting the final filename, but a symlinked parent
  can direct a new file outside `upload_path`.
- DELETE through a symlinked parent can delete an outside file. A final-component
  symlink deletes the link itself.

These application paths remain unchanged while application integration is
handled separately. A lexical-prefix comparison alone cannot fix this. The
application work must enforce containment during actual access, including index
files, listing directories, upload parents, and CGI script handoff. Avoid checking
and then reopening the original untrusted path: a symlink can be replaced between
those operations. Respect the subject's allowed-function list.

Reproduce with harmless temporary fixtures: configure a root and a separate
outside directory, place a symlink named `escape` inside the root pointing to
the outside directory, and request `/escape/sentinel.txt`. With an upload route,
POST and DELETE through that parent link also escape their configured roots.
Use disposable sentinels. After the application fix, require rejection and
verify that the outside files remain unchanged.

The disposable fixture check returned 200 for outside GET and CGI execution,
and 201 for an outside upload. To repeat the diagnostic alongside CGI tests:

```sh
python3 tests/run_cgi_server_tests.py --review-symlinks
```

This diagnostic demonstrates the current gap; it is not a passing containment
test. Normal upload and DELETE regression checks use paths inside their roots.

## Reserved files

`src/Cgi.cpp`, `src/RequestHandler.cpp`, `src/HttpParser.cpp`, and
`src/ConfigParser.cpp` were not edited. Integration uses the existing interfaces
and the network/main handoff. No branches were merged; no commits or pushes were made.
