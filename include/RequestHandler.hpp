#pragma once

#include "Config.hpp"
#include "HttpTypes.hpp"

class RequestHandler
{
public:
    HTTPResponse handleRequest(
        const HTTPRequest& request,
        const ServerConfig& serverConfig);

private:
    const LocationConfig* findLocation(
        const std::string& uri,
        const ServerConfig& serverConfig) const;

    bool isLocationMatch(
        const std::string& uri,
        const std::string& locationPath) const;

    bool isSupportedMethod(
        const std::string& method) const;

    bool isMethodAllowed(
        const std::string& method,
        const LocationConfig& location) const;

    std::string buildAllowHeader(
        const LocationConfig& location) const;

    HTTPResponse handleGet(
        const HTTPRequest& request,
        const LocationConfig& location);

    HTTPResponse handlePost(
        const HTTPRequest& request,
        const LocationConfig& location);

    HTTPResponse handleDelete(
        const HTTPRequest& request,
        const LocationConfig& location);

    HTTPResponse makeErrorResponse(
        int statusCode,
        const std::string& message) const;
};