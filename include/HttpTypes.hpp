#pragma once

#include <map>
#include <string>

struct HTTPRequest
{
    std::string method; // GET, POST or DELETE
    std::string uri; // Resource path requested by client, example: /uploads/cat.txt
    std::string body; // Data sent by client, mainly used with POST, example: file content
    std::map<std::string, std::string> headers; // Request headers, example: "Host" -> "localhost:8080"
};

struct HTTPResponse
{
    int statusCode; // Result code returned to client, example: 200, 404 or 405
    std::string body; // Content returned to client, example: HTML page or file content
    std::map<std::string, std::string> headers; // Response headers, example: "Content-Type" -> "text/html"
    HTTPResponse() : statusCode(200) {}
};