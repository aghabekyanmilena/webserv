#include "RequestHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstddef>
#include <dirent.h>


bool RequestHandler::isLocationMatch(const std::string& uri, const std::string& locationPath) const
{
    //uri = client-i tvacna, locationPath-y mer serveri configna
    if (locationPath == "/") //default root location
        return true; //location-y okaya
    if (uri == locationPath)
        return true;        
    if (uri.size() <= locationPath.size()) //"/img", "/images"
        return false;
    if (uri.compare(0, locationPath.size(), locationPath) != 0) //"/uploads/cat.txt", "/uploads"
        return false;
    return (uri[locationPath.size()] == '/');
}

const LocationConfig* RequestHandler::findLocation(const std::string& uri, const ServerConfig& serverConfig) const
{
    const LocationConfig* bestMatch = NULL;
    std::size_t bestLength = 0;

    for (std::size_t i = 0; i < serverConfig.locations.size(); ++i)
    {
        const LocationConfig& location = serverConfig.locations[i];
        if (isLocationMatch(uri, location.path) && location.path.size() >= bestLength)
        {
            bestMatch = &location;
            bestLength = location.path.size();
        }
    }
    return bestMatch;
}

bool RequestHandler::isSupportedMethod(const std::string& method) const
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

bool RequestHandler::isMethodAllowed(const std::string& method, const LocationConfig& location) const
{
    for (std::size_t i = 0; i < location.allowedMethods.size(); ++i)
    {
        if (location.allowedMethods[i] == method)
            return true;
    }
    return false;
}

std::string RequestHandler::buildAllowHeader(const LocationConfig& location) const
{
    std::string result;

    for (std::size_t i = 0; i < location.allowedMethods.size(); ++i)
    {
        if (i != 0)
            result += ", ";
        result += location.allowedMethods[i];
    }
    return result;
}

HTTPResponse RequestHandler::makeErrorResponse(int statusCode, const std::string& message) const
{
    HTTPResponse response;

    response.statusCode = statusCode;
    response.body = "<html><body><h1>" + message + "</h1></body></html>";
    response.headers["Content-Type"] = "text/html";
    return response;
}

HTTPResponse RequestHandler::handleRequest(const HTTPRequest& request, const ServerConfig& serverConfig)
{
    const LocationConfig* location = findLocation(request.uri, serverConfig);
    if (location == NULL)
        return makeErrorResponse(404, "404 Not Found");
    if (!isSupportedMethod(request.method))
        return makeErrorResponse(501, "501 Not Implemented");
    if (!isMethodAllowed(request.method, *location))
    {
        HTTPResponse response = makeErrorResponse(405, "405 Method Not Allowed");
        response.headers["Allow"] = buildAllowHeader(*location);
        return response;
    }
    if (location->hasRedirect)
    {
        HTTPResponse response;
        response.statusCode = location->redirectCode;
        response.headers["Location"] = location->redirectTarget;
        return response;
    }
    if (request.method == "GET")
        return handleGet(request, *location);
    if (request.method == "POST")
        return handlePost(request, *location);
    if (request.method == "DELETE")
        return handleDelete(request, *location);
    return makeErrorResponse(500, "500 Internal Server Error");
}

HTTPResponse RequestHandler::handleGet(const HTTPRequest& request, const LocationConfig& location)
{
    std::string relativePath = request.uri;

    if (location.path != "/" && relativePath.compare(0, location.path.size(), location.path) == 0)
        relativePath = relativePath.substr(location.path.size());
    if (relativePath.empty())
        relativePath = "/";
    std::string filePath = location.root + relativePath;

    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) != 0)
        return makeErrorResponse(404, "404 Not Found");

    if (S_ISDIR(fileInfo.st_mode))
    {
        if (request.uri.empty() || request.uri[request.uri.size() - 1] != '/')
        {
            HTTPResponse response;
            response.statusCode = 301;
            response.headers["Location"] = request.uri + "/";
            return response;
        }
        if (!filePath.empty() && filePath[filePath.size() - 1] != '/')
            filePath += "/";
        if (!location.index.empty())
        {
            std::string indexPath = filePath + location.index;
            struct stat indexInfo;

            if (stat(indexPath.c_str(), &indexInfo) == 0 && S_ISREG(indexInfo.st_mode))
            {
                filePath = indexPath;
                fileInfo = indexInfo;
            }
            else if (!location.autoindex)
                return makeErrorResponse(403, "403 Forbidden");
        }
        else if (!location.autoindex)
            return makeErrorResponse(403, "403 Forbidden");
        if (S_ISDIR(fileInfo.st_mode) && location.autoindex)
        {
            DIR* directory = opendir(filePath.c_str());
            if (directory == NULL)
                return makeErrorResponse(403, "403 Forbidden");
            
            std::ostringstream html;
            html << "<!DOCTYPE html>\n";
            html << "<html>\n";
            html << "<head>\n";
            html << "    <title>Index of "
                 << request.uri
                 << "</title>\n";
            html << "</head>\n";
            html << "<body>\n";
            html << "    <h1>Index of "
                 << request.uri
                 << "</h1>\n";
            html << "    <ul>\n";

            struct dirent* entry;
            while ((entry = readdir(directory)) != NULL)
            {
                std::string name = entry->d_name;
                if (name == "." || name == "..")
                    continue;

                std::string entryPath = filePath + name;
                struct stat entryInfo;
                bool isDirectory = false;

                if (stat(entryPath.c_str(), &entryInfo) == 0 && S_ISDIR(entryInfo.st_mode))
                    isDirectory = true;
                html << "        <li><a href=\""
                     << name;

                if (isDirectory)
                    html << "/";

                html << "\">"
                     << name;

                if (isDirectory)
                    html << "/";

                html << "</a></li>\n";
            }

            closedir(directory);
            html << "    </ul>\n";
            html << "</body>\n";
            html << "</html>\n";

            HTTPResponse response;
            response.statusCode = 200;
            response.body = html.str();
            response.headers["Content-Type"] =
                "text/html";
            return response;
        }
    }

    if (!S_ISREG(fileInfo.st_mode))
        return makeErrorResponse(403, "403 Forbidden");

    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return makeErrorResponse(403, "403 Forbidden");

    std::ostringstream content;
    content << file.rdbuf();
    file.close();

    HTTPResponse response;
    response.statusCode = 200;
    response.body = content.str();
    response.headers["Content-Type"] = "text/html";
    return response;
}

HTTPResponse RequestHandler::handlePost(const HTTPRequest& request, const LocationConfig& location)
{
    (void)request;
    (void)location;

    HTTPResponse response;
    response.statusCode = 200;
    response.body = "POST is allowed";
    return response;
}

HTTPResponse RequestHandler::handleDelete(const HTTPRequest& request, const LocationConfig& location)
{
    (void)request;
    (void)location;

    HTTPResponse response;
    response.statusCode = 200;
    response.body = "DELETE is allowed";
    return response;
}