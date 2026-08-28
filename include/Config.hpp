#pragma once

#include <map>
#include <string>
#include <vector>

struct LocationConfig
{
    std::string path;
    std::string root;
    std::string index;
    bool autoindex;

    std::vector<std::string> allowedMethods;

    bool hasRedirect;
    int redirectCode;
    std::string redirectTarget;

    std::string uploadDirectory;

    LocationConfig()
        : autoindex(false),
          hasRedirect(false),
          redirectCode(0)
    {
    }
};

struct ServerConfig
{
    std::vector<LocationConfig> locations;
    std::map<int, std::string> errorPages;
    std::size_t maxBodySize;

    ServerConfig()
        : maxBodySize(0)
    {
    }
};