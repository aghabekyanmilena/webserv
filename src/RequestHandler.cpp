#include "RequestHandler.hpp"

HTTPResponse RequestHandler::handleRequest(const HTTPRequest& originalRequest, const ServerConfig& serverConfig)
{
    HTTPRequest request = originalRequest;
    if (!decodeUriPath(originalRequest.uri, request.uri))
        return makeErrorResponse(400, "400 Bad Request");
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    if (request.body.size() > serverConfig.getMaxBodySize())
        return makeErrorResponse(413, "413 Payload Too Large");
    const Location* location = findLocation(request.uri, serverConfig); // serverConfigi mejic gtnuma clineti uri-y
    if (location == NULL)
        return makeErrorResponse(404, "404 Not Found");
    if (!isSupportedMethod(request.method)) // checka anum methody ka te che
        return makeErrorResponse(501, "501 Not Implemented");
    if (!isMethodAllowed(request.method, *location)) // allow araca et methody te che
    {
        HTTPResponse response = makeErrorResponse(405, "405 Method Not Allowed"); // ka methody bayc allow arac chi et locationum
        response.headers["Allow"] = buildAllowHeader(*location); // asuma voronqa allow tvac
        return response;
    }
    if (location->hasRedirect()) // ardyoq locationy redirecta?
    {
        HTTPResponse response;
        response.statusCode = location->getRedirectCode();
        response.headers["Location"] = location->getRedirectTarget(); // asuma clientin ur piti gna
        return response;
    }
    const std::string& cgiExtension = location->getCgiExtension();
    if (!cgiExtension.empty() && request.uri.size() >= cgiExtension.size() && request.uri.compare(request.uri.size() - cgiExtension.size(), cgiExtension.size(), cgiExtension) == 0 && (request.method == "GET" || request.method == "POST"))
        return makeErrorResponse(501, "501 CGI Integration Required");
    if (request.method == "GET")
        return handleGet(request, *location);
    if (request.method == "POST")
        return handlePost(request, *location);
    if (request.method == "DELETE")
        return handleDelete(request, *location);
    return makeErrorResponse(500, "500 Internal Server Error");
}
