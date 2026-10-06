#pragma once

#include "HttpTypes.hpp"

// Shared text, URI and error-response helpers for the application layer.
class RequestUtils
{
    protected:
        std::string escapeHtml(const std::string& text) const;
        std::string encodePathSegment(const std::string& name) const;
        bool decodeUriPath(const std::string& encoded, std::string& decoded) const;
        bool isPathSafe(const std::string& path) const;
        std::string encodeUriPath(const std::string& path) const;
        HTTPResponse makeErrorResponse(int statusCode, const std::string& message) const;
};
