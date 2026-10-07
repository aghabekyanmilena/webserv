#pragma once

#include <string>
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"

class ResponseBuilder
{
public:
    static std::string serialize(const HttpResponse& response);
    static HttpResponse makeError(int statusCode, const std::string& body);
    static HttpResponse makeError(int statusCode, const std::string& body,
                                  const ServerConfig& config);
    static std::string reasonPhrase(int statusCode);
    static std::string defaultErrorBody(int statusCode);
};
