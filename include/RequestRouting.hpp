#pragma once

#include "RequestUtils.hpp"
#include "Location.hpp"
#include "ServerConfig.hpp"

class RequestRouting : public RequestUtils
{
    protected:
        bool isLocationMatch(const std::string& uri, const std::string& locationPath) const;
        const Location* findLocation(const std::string& uri, const ServerConfig& serverConfig) const;
        std::string getRelativePath(const std::string& uri, const Location& location) const;
        std::string joinPath(const std::string& directory, const std::string& relativePath) const;
        std::string buildFilePath(const std::string& uri, const Location& location) const;
};
