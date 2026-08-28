#include "RequestHandler.hpp"

#include <cstddef>

bool RequestHandler::isLocationMatch(
    const std::string& uri,
    const std::string& locationPath) const
{
    if (locationPath == "/")
        return true;

    if (uri == locationPath)
        return true;

    if (uri.size() <= locationPath.size())
        return false;

    if (uri.compare(0, locationPath.size(), locationPath) != 0)
        return false;

    return uri[locationPath.size()] == '/';
}

const LocationConfig* RequestHandler::findLocation(
    const std::string& uri,
    const ServerConfig& serverConfig) const
{
    const LocationConfig* bestMatch = NULL;
    std::size_t bestLength = 0;

    for (std::size_t i = 0;
         i < serverConfig.locations.size();
         ++i)
    {
        const LocationConfig& location =
            serverConfig.locations[i];

        if (isLocationMatch(uri, location.path)
            && location.path.size() >= bestLength)
        {
            bestMatch = &location;
            bestLength = location.path.size();
        }
    }

    return bestMatch;
}

bool RequestHandler::isSupportedMethod(
    const std::string& method) const
{
    return method == "GET"
        || method == "POST"
        || method == "DELETE";
}

bool RequestHandler::isMethodAllowed(
    const std::string& method,
    const LocationConfig& location) const
{
    for (std::size_t i = 0;
         i < location.allowedMethods.size();
         ++i)
    {
        if (location.allowedMethods[i] == method)
            return true;
    }

    return false;
}

std::string RequestHandler::buildAllowHeader(
    const LocationConfig& location) const
{
    std::string result;

    for (std::size_t i = 0;
         i < location.allowedMethods.size();
         ++i)
    {
        if (i != 0)
            result += ", ";

        result += location.allowedMethods[i];
    }

    return result;
}

HTTPResponse RequestHandler::makeErrorResponse(
    int statusCode,
    const std::string& message) const
{
    HTTPResponse response;

    response.statusCode = statusCode;
    response.body = "<html><body><h1>"
        + message
        + "</h1></body></html>";

    response.headers["Content-Type"] = "text/html";

    return response;
}

HTTPResponse RequestHandler::handleRequest(
    const HTTPRequest& request,
    const ServerConfig& serverConfig)
{
    const LocationConfig* location =
        findLocation(request.uri, serverConfig);

    if (location == NULL)
        return makeErrorResponse(404, "404 Not Found");

    if (!isSupportedMethod(request.method))
        return makeErrorResponse(501, "501 Not Implemented");

    if (!isMethodAllowed(request.method, *location))
    {
        HTTPResponse response =
            makeErrorResponse(405, "405 Method Not Allowed");

        response.headers["Allow"] =
            buildAllowHeader(*location);

        return response;
    }

    if (location->hasRedirect)
    {
        HTTPResponse response;

        response.statusCode = location->redirectCode;
        response.headers["Location"] =
            location->redirectTarget;

        return response;
    }

    if (request.method == "GET")
        return handleGet(request, *location);

    if (request.method == "POST")
        return handlePost(request, *location);

    if (request.method == "DELETE")
        return handleDelete(request, *location);

    return makeErrorResponse(500, "500 Internal Server Error");
}

HTTPResponse RequestHandler::handleGet(
    const HTTPRequest& request,
    const LocationConfig& location)
{
    (void)request;
    (void)location;

    HTTPResponse response;

    response.statusCode = 200;
    response.body = "GET is allowed";

    return response;
}

HTTPResponse RequestHandler::handlePost(
    const HTTPRequest& request,
    const LocationConfig& location)
{
    (void)request;
    (void)location;

    HTTPResponse response;

    response.statusCode = 200;
    response.body = "POST is allowed";

    return response;
}

HTTPResponse RequestHandler::handleDelete(
    const HTTPRequest& request,
    const LocationConfig& location)
{
    (void)request;
    (void)location;

    HTTPResponse response;

    response.statusCode = 200;
    response.body = "DELETE is allowed";

    return response;
}