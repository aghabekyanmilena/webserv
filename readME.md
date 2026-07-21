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
