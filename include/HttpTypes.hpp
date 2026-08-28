#pragma once

#include <map>
#include <string>

struct HTTPRequest
{
    std::string method;
    std::string uri;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HTTPResponse
{
    int statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
    HTTPResponse() : statusCode(200) {}
};