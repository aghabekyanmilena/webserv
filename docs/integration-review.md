# Application and CGI integration review

The network integration uses `RequestCgi::prepareCgi()` and
`RequestCgi::parseCgiOutput()`. The HTTP parser's raw query and decoded entity
body reach the plan; chunk framing is removed before stdin is written. CGI
status, headers, and content length use that parser and `ResponseBuilder`.
Valid CGI entity bodies are preserved, including empty error bodies.

The unused legacy CGI implementation has been removed. CGI context comes from
`CgiContext.hpp`, and the active implementation uses `RequestCgi` and `CgiProcess`.

## Root containment

The initial review found that GET, CGI, uploads, and DELETE could follow symlinks
outside their configured roots. Those paths now use `RootedPath`:

- Every request-derived path component is opened with `O_PATH | O_NOFOLLOW`.
  `stat` on the resulting descriptor identifies and rejects symbolic links.
- Directory descriptors anchor subsequent operations through `/proc/self/fd`,
  so replacing an earlier pathname does not redirect access to another directory.
- GET reads the pinned regular-file inode. Directory listings and index lookup
  use pinned directories; listings omit symlink entries.
- Uploads create the final file exclusively inside the pinned parent. DELETE
  removes an entry inside its pinned parent and cannot follow a final symlink.
- CGI preparation retains the script and directory descriptors in `CgiRequest`.
  The child changes to that directory and executes its interpreter against the
  pinned script. It receives the effective index URI in `SCRIPT_NAME` when the
  requested directory has a CGI index.

The configured root itself is trusted and may contain symlinks. Symlinks below
that root are rejected even when their targets are inside the root. This uses
Linux procfs and the subject-listed `open`, `stat`, `dup`, and `close` functions;
it introduces no `realpath`, `readlink`, `openat`, or `fcntl` calls. Descriptor
ownership closes files on allocation failures as well as normal returns.

Reproduce with harmless temporary fixtures: configure a root and a separate
outside directory, place a symlink named `escape` inside the root pointing to
the outside directory, and request `/escape/sentinel.txt`. With an upload route,
POST and DELETE through that parent link also escape their configured roots.
Use disposable sentinels. Requests must be rejected and the outside files must
remain unchanged.

The Bash/C++ regression checks cover outside file and parent links, static and
CGI indices, uploads, DELETE, and replaced file/directory pathnames:

```sh
bash tests/manual_bug_checks.sh
```

The checks also confirm that ordinary static files, CGI working directories,
and upload/DELETE operations continue to work.

## Integration scope

CGI uses the existing network/main handoff and single poll loop. HTTP request validation rejects invalid method tokens,
Host values, header controls, and bare header newlines. Response serialization
omits bodies and Content-Length for 204 responses. Uploads supply Location, and
PDF files use application/pdf. No custom namespaces are defined in the server.
No branches were merged; no commits or pushes were made.
