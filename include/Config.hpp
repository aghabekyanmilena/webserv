#pragma once

#include <map>
#include <string>
#include <vector>

struct LocationConfig
{
    std::string path; // URI location from config, example: /uploads
    std::string root; // Real directory on server, example: ./www/uploads
    std::string index; // Default file returned for a directory, example: index.html
    bool autoindex; // If no index file exists: true displays directory files, false returns 403

    std::vector<std::string> allowedMethods; //GET POST DELETE

    bool hasRedirect; // True if this location redirects the client to another URI
    int redirectCode; // HTTP redirect status code, example: 301 or 302
    std::string redirectTarget; // URI where the client is redirected, example: /new-page
    std::string uploadDirectory; // Directory where uploaded files are saved, example: ./www/uploads

    LocationConfig() : autoindex(false), hasRedirect(false), redirectCode(0) {}
};

struct ServerConfig
{
    std::vector<LocationConfig> locations; // Stores all location rules, example: /, /uploads, /images
    std::map<int, std::string> errorPages; // Connects an error status to its HTML file, example: 404 -> ./www/errors/404.html
    std::size_t maxBodySize; // Maximum allowed request body/upload size in bytes, example: 1000000 bytes

    ServerConfig() : maxBodySize(0) {}
};