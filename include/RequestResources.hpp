#pragma once

#include "RequestRouting.hpp"

class RequestResources : public RequestRouting
{
    protected:
        HTTPResponse handleGet(const HTTPRequest& request, const Location& location);
        HTTPResponse handlePost(const HTTPRequest& request, const Location& location);
        HTTPResponse handleDelete(const HTTPRequest& request, const Location& location);
        std::string getMimeType(const std::string& filePath) const;
};
