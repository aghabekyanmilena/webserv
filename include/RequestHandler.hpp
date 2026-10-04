#pragma once

#include "Config.hpp"
#include "HttpTypes.hpp"
#include "Location.hpp"

class RequestHandler
{
    public:
        HTTPResponse handleRequest(const HTTPRequest& request, const ServerConfig& serverConfig);  // Receives request, and returns response.

    private:
        std::string getMimeType(const std::string& filePath) const;
        const Location* findLocation(const std::string& uri, const ServerConfig& serverConfig) const; // Finds the longest matching location for the requested URI.
        bool isLocationMatch(const std::string& uri, const std::string& locationPath) const; // Checks whether a URI belongs to a location.
        bool isSupportedMethod(const std::string& method) const; // Checks whether Webserv supports the method: GET, POST or DELETE.
        bool isMethodAllowed(const std::string& method, const Location& location) const;  // Checks whether the method is allowed in the selected location.
        std::string buildAllowHeader(const Location& location) const;  // Creates the Allow header value, example: "GET, POST".
        HTTPResponse handleGet(const HTTPRequest& request, const Location& location);  // Handles GET requests: files, directories, index and autoindex.
        HTTPResponse handlePost(const HTTPRequest& request, const Location& location); // Handles POST requests and uploaded data.
        HTTPResponse handleDelete(const HTTPRequest& request, const Location& location);  // Handles DELETE requests and removes resources.
        HTTPResponse makeErrorResponse(int statusCode, const std::string& message) const; // Creates an error response, example: 404 Not Found.
        std::string buildFilePath(const std::string& uri, const Location& location) const; // Converts a client URI into a path. /uploads/cat.txt->./www/uploads/cat.txt
        bool isPathSafe(const std::string& path) const;
};