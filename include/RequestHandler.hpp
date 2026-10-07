#pragma once

#include "RequestCgi.hpp"

class RequestHandler : public RequestCgi
{
    public:
        HTTPResponse handleRequest(const HTTPRequest& request, const ServerConfig& serverConfig);
};
