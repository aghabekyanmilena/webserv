#include "RequestResources.hpp"
#include "MultipartUpload.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>

namespace
{
    class DirectoryGuard
    {
        DIR* _directory;
        DirectoryGuard(const DirectoryGuard&);
        DirectoryGuard& operator=(const DirectoryGuard&);
    public:
        explicit DirectoryGuard(DIR* directory) : _directory(directory) {}
        ~DirectoryGuard() { closedir(_directory); }
    };
}

HTTPResponse RequestResources::handleGet(const HTTPRequest& request, const Location& location)
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
            response.headers["Location"] = encodeUriPath(request.uri) + "/";
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
            
            DirectoryGuard directoryGuard(directory);
            std::ostringstream html;
            html << "<!DOCTYPE html>\n";
            html << "<html>\n";
            html << "<head>\n";
            html << "    <title>Index of "
                 << escapeHtml(request.uri)
                 << "</title>\n";
            html << "</head>\n";
            html << "<body>\n";
            html << "    <h1>Index of "
                 << escapeHtml(request.uri)
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
                html << "        <li><a href=\"" << encodePathSegment(name);
                if (isDirectory)
                    html << "/";
                html << "\">" << escapeHtml(name);
                if (isDirectory)
                    html << "/";
                html << "</a></li>\n";
            }


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

    const std::string& extension = location.getCgiExtension();
    if (!extension.empty() && filePath.size() >= extension.size() &&
        filePath.compare(filePath.size() - extension.size(), extension.size(), extension) == 0)
        return makeErrorResponse(501, "501 CGI Integration Required");

    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return makeErrorResponse(403, "403 Forbidden");

    std::ostringstream content;
    content << file.rdbuf();
    if (file.bad())
        return makeErrorResponse(500, "500 Internal Server Error");
    file.close();

    HTTPResponse response;
    response.statusCode = 200;
    response.body = content.str();
    response.headers["Content-Type"] = getMimeType(filePath);
    return response;
}

HTTPResponse RequestResources::handlePost(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    if (location.getUploadDirectory().empty())
        return makeErrorResponse(403, "403 Forbidden");

    std::string relativePath = getRelativePath(request.uri, location);
    std::string data = request.body;
    std::map<std::string, std::string>::const_iterator type = request.headers.find("content-type");
    if (type != request.headers.end() &&
        MultipartUpload::lower(MultipartUpload::trim(type->second.substr(0, type->second.find(';')))) == "multipart/form-data")
    {
        MultipartUpload upload;
        if (!upload.parse(type->second, request.body))
            return makeErrorResponse(400, "400 Bad Request");
        // Form uploads target the upload route itself; the filename comes from the form.
        if (!relativePath.empty())
            return makeErrorResponse(400, "400 Bad Request");
        relativePath = upload.filename;
        data.swap(upload.body);
    }
    if (relativePath.empty() || relativePath[relativePath.size() - 1] == '/')
        return makeErrorResponse(400, "400 Bad Request");

    const std::string filePath = joinPath(location.getUploadDirectory(), relativePath);
    const std::size_t slash = filePath.rfind('/');
    const std::string parent = slash == 0 ? "/" : filePath.substr(0, slash);
    struct stat directoryInfo;
    if (stat(parent.c_str(), &directoryInfo) != 0 || !S_ISDIR(directoryInfo.st_mode))
        return makeErrorResponse(404, "404 Not Found");
    if (access(parent.c_str(), W_OK) != 0)
        return makeErrorResponse(403, "403 Forbidden");
    // Exclusive creation avoids overwriting an existing file or following a final symlink.
    const int fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0)
    {
        // errno is inspected only after open(), never after read()/write().
        const int openError = errno;
        if (openError == EEXIST || openError == EACCES || openError == EPERM)
            return makeErrorResponse(403, "403 Forbidden");
        if (openError == ENOENT || openError == ENOTDIR)
            return makeErrorResponse(404, "404 Not Found");
        return makeErrorResponse(500, "500 Internal Server Error");
    }
    std::size_t written = 0;
    bool failed = false;
    while (written < data.size())
    {
        const std::size_t remaining = data.size() - written;
        const std::size_t count = remaining < 8192 ? remaining : 8192;
        // Regular disk files are exempt from the subject's poll readiness requirement.
        const ssize_t result = write(fd, data.data() + written, count);
        if (result <= 0) { failed = true; break; }
        written += static_cast<std::size_t>(result);
    }
    if (close(fd) != 0) failed = true;
    if (failed)
    {
        std::remove(filePath.c_str());
        return makeErrorResponse(500, "500 Internal Server Error");
    }
    HTTPResponse response;
    response.statusCode = 201;
    response.body = "File uploaded successfully";
    response.headers["Content-Type"] = "text/plain";
    return response;
}

HTTPResponse RequestResources::handleDelete(const HTTPRequest& request, const Location& location)
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

std::string RequestResources::getMimeType(const std::string& filePath) const
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
