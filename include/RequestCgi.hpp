#pragma once

#include "RequestResources.hpp"
#include "CgiRequest.hpp"

class RequestCgi : public RequestResources
{
    public:
        enum CgiResult { CGI_NOT_SELECTED, CGI_READY, CGI_ERROR };
        CgiResult prepareCgi(const HTTPRequest& request, const ServerConfig& config,
            const CgiContext& context, CgiRequest& plan, HTTPResponse& error) const;

        // Call after CgiProcess has finished successfully (stdout reached EOF).
        HTTPResponse parseCgiOutput(const std::string& output) const;

    protected:
        bool isSupportedMethod(const std::string& method) const;
        bool isMethodAllowed(const std::string& method, const Location& location) const;
        std::string buildAllowHeader(const Location& location) const;
};
