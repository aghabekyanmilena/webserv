#include "RequestHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstddef>
#include <dirent.h>
#include <cstdio>


bool RequestHandler::isLocationMatch(const std::string& uri, const std::string& locationPath) const
{
    //uri = client-i tvacna, locationPath-y mer serveri configica
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

const Location* RequestHandler::findLocation(const std::string& uri, const ServerConfig& serverConfig) const
{
    const Location* bestMatch = NULL;
    std::size_t bestLength = 0;

    for (std::size_t i = 0; i < serverConfig.getLocations().size(); ++i)
    {
        const Location& location = serverConfig.getLocations()[i];
        if (isLocationMatch(uri, location.getPath()) && location.getPath().size() >= bestLength)
        {
            bestMatch = &location;
            bestLength = location.getPath().size();
        }
    }
    return bestMatch;
}

bool RequestHandler::isSupportedMethod(const std::string& method) const
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

bool RequestHandler::isMethodAllowed(const std::string& method, const Location& location) const
{
    for (std::size_t i = 0; i < location.getAllowedMethods().size(); ++i)
    {
        if (location.getAllowedMethods()[i] == method)
            return true;
    }
    return false;
}

std::string RequestHandler::buildAllowHeader(const Location& location) const
{
    std::string result;

    for (std::size_t i = 0; i < location.getAllowedMethods().size(); ++i)
    {
        if (i != 0)
            result += ", ";
        result += location.getAllowedMethods()[i];
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
    const Location* location = findLocation(request.uri, serverConfig); // serverConfigi mejic gtnuma clineti uri-y
    if (location == NULL)
        return makeErrorResponse(404, "404 Not Found");
    if (!isSupportedMethod(request.method)) // checka anum methody ka te che
        return makeErrorResponse(501, "501 Not Implemented");
    if (!isMethodAllowed(request.method, *location)) // allow araca et methody te che
    {
        HTTPResponse response = makeErrorResponse(405, "405 Method Not Allowed"); // ka methody bayc allow arac chi et locationum
        response.headers["Allow"] = buildAllowHeader(*location); // asuma voronqa allow tvac
        return response;
    }
    if (location->hasRedirect()) // ardyoq locationy redirecta?
    {
        HTTPResponse response;
        response.statusCode = location->getRedirectCode();
        response.headers["Location"] = location->getRedirectTarget(); // asuma clientin ur piti gna
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

HTTPResponse RequestHandler::handleGet(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    std::string filePath = buildFilePath(request.uri, location);

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
        if (!location.getIndex().empty())
        {
            std::string indexPath = filePath + location.getIndex();
            struct stat indexInfo;

            if (stat(indexPath.c_str(), &indexInfo) == 0 && S_ISREG(indexInfo.st_mode))
            {
                filePath = indexPath;
                fileInfo = indexInfo;
            }
            else if (!location.getAutoindex())
                return makeErrorResponse(403, "403 Forbidden");
        }
        else if (!location.getAutoindex())
            return makeErrorResponse(403, "403 Forbidden");
        if (S_ISDIR(fileInfo.st_mode) && location.getAutoindex())
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
                html << "        <li><a href=\"" << name;
                if (isDirectory)
                    html << "/";
                html << "\">" << name;
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
            response.headers["Content-Type"] = "text/html";
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
    response.headers["Content-Type"] = getMimeType(filePath);
    return response;
}

HTTPResponse RequestHandler::handlePost(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");

    if (request.uri == location.getPath() || request.uri == location.getPath() + "/")
        return makeErrorResponse(400, "400 Bad Request");

    if (location.getUploadDirectory().empty())
        return makeErrorResponse(403, "403 Forbidden");

    std::string filePath = location.getUploadDirectory() + request.uri.substr(location.getPath().size());
    std::ofstream file(filePath.c_str(), std::ios::out | std::ios::binary);

    if (!file.is_open())
        return makeErrorResponse(500, "500 Internal Server Error");

    file << request.body;
    file.close();
    if (!file)
        return makeErrorResponse(500, "500 Internal Server Error");

    HTTPResponse response;
    response.statusCode = 201;
    response.body = "File uploaded successfully";
    response.headers["Content-Type"] = "text/plain";
    return response;
}

HTTPResponse RequestHandler::handleDelete(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    std::string filePath = buildFilePath(request.uri, location);

    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) != 0)
        return makeErrorResponse(404, "404 Not Found");

    if (!S_ISREG(fileInfo.st_mode))
        return makeErrorResponse(403, "403 Forbidden");

    if (std::remove(filePath.c_str()) != 0)
        return makeErrorResponse(500, "500 Internal Server Error");

    HTTPResponse response;
    response.statusCode = 204;
    response.body = "";
    return response;
}

std::string RequestHandler::getMimeType(const std::string& filePath) const
{
    std::size_t dot = filePath.rfind('.');
    
    if (dot == std::string::npos)
        return "application/octet-stream";

    std::string extension = filePath.substr(dot);

    if (extension == ".html" || extension == ".htm")
        return "text/html";
    if (extension == ".css")
        return "text/css";
    if (extension == ".js")
        return "application/javascript";
    if (extension == ".txt")
        return "text/plain";
    if (extension == ".jpg" || extension == ".jpeg")
        return "image/jpeg";
    if (extension == ".png")
        return "image/png";
    if (extension == ".gif")
        return "image/gif";
    if (extension == ".svg")
        return "image/svg+xml";

    return "application/octet-stream";
}

std::string RequestHandler::buildFilePath(const std::string& uri, const Location& location) const
{
    std::string relativePath = uri;

    if (location.getPath() != "/" && relativePath.compare(0, location.getPath().size(), location.getPath()) == 0)
        relativePath = relativePath.substr(location.getPath().size());
    if (relativePath.empty())
        relativePath = "/";
    return location.getRoot() + relativePath;
}

bool RequestHandler::isPathSafe(const std::string& path) const
{
    if (path == "..")
        return false;
    if (path.find("../") != std::string::npos)
        return false;
    if (path.find("/..") != std::string::npos)
        return false;
    return true;
}