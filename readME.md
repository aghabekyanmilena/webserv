Person 1 — Networking (lowest layer) (Anush)

Owns

socket(), bind(), listen(), accept(), poll(), recv(), send(), client management, event loop, multiple ports, connection timeout, nonblocking sockets

Produces
raw HTTP request string

Needs to expose
std::string receiveRequest(fd);
void sendResponse(fd, response);

This person never parses HTTP.

Person 2 — HTTP + Configuration (Milena)

Owns

configuration parser, HTTP parser, Request object, Response object, status codes, headers, error pages, response formatting

Produces
HTTPRequest
HTTPResponse

Never touches sockets.

Person 3 — Business Logic (Arina)

Owns

routing, GET, POST, DELETE, directory listing, uploads, CGI, file, access, autoindex, redirections

This person receives
HTTPRequest

Returns
HTTPResponse

Never touches poll().


Configuration (Person 2)

Build with `make` (C++98, -Wall -Wextra -Werror).
Run `./webserv` to parse `webserv.conf`, or
`./webserv configs/multiple-servers.conf` for two websites on three ports.
The current main prints parsed settings; networking and route execution are
still the other team members' responsibilities.

`include/ConfigParser.hpp` declares the parser and `src/ConfigParser.cpp`
implements it without namespace declarations. `Config::parseFile()` exposes
validated server configurations through `Config::getServers()` and preserves
the previous configuration if parsing fails.

Server directives:
- `host`: interface address, default `127.0.0.1`.
- `listen`: port 1..65535; repeat for multiple ports on the same interface.
- `server_name`: optional name.
- `root`: default filesystem directory inherited by locations.
- `client_max_body_size`: maximum body size in bytes, default 1000000.
- `error_page`: status 400..599 followed by a file path.

Location directives:
- `allowed_methods`: GET, POST and/or DELETE; default GET.
- `root`: filesystem directory overriding the server root.
- `index`: default filename for a directory request.
- `autoindex`: on/off; default off.
- `return`: redirect status 300..399 followed by a target.
- `upload_path`: upload storage directory.
- `cgi_extension` and `cgi_path`: file extension and interpreter path;
  configure both together.

Directives end with semicolons; server and location blocks use braces.
Comments start with `#`. Unknown directives, duplicate singleton directives,
duplicate ports/routes/error statuses, invalid numbers and malformed blocks
are rejected with line-numbered errors. Paths in these examples are relative
to the process working directory. The parser does not create directories,
serve error pages, enforce HTTP body limits or execute CGI; it supplies those
settings to the networking and business logic layers. The example `www`
directories and files must be supplied when those layers are implemented.
