#include "RequestResources.hpp"
#include "MultipartUpload.hpp"
#include "RootedPath.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <cctype>

class RequestResources::DirectoryGuard
{
    DIR* _directory;
    DirectoryGuard(const DirectoryGuard&);
    DirectoryGuard& operator=(const DirectoryGuard&);
public:
    explicit DirectoryGuard(DIR* directory) : _directory(directory) {}
    ~DirectoryGuard() { closedir(_directory); }
};

HTTPResponse RequestResources::handleGet(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    RootedPath path;
    int status = 500;
    struct stat fileInfo;
    if (!path.resolve(location.getRoot(), getRelativePath(request.uri, location), status)
        || !path.inspect(fileInfo, status))
        return makeErrorResponse(status, "File access denied or unavailable");
    std::string filePath = buildFilePath(request.uri, location);

    if (S_ISDIR(fileInfo.st_mode))
    {
        if (request.uri.empty() || request.uri[request.uri.size() - 1] != '/')
        {
            HTTPResponse response;
            response.statusCode = 301;
            response.headers["Location"] = encodeUriPath(request.uri) + "/";
            return response;
        }
        if (!location.getIndex().empty())
        {
            RootedPath indexPath;
            struct stat indexInfo;
            if (indexPath.resolve(RootedPath::descriptorPath(path.targetFd()), location.getIndex(), status)
                && indexPath.inspect(indexInfo, status) && S_ISREG(indexInfo.st_mode))
            {
                path = indexPath;
                filePath = joinPath(filePath, location.getIndex());
                fileInfo = indexInfo;
            }
            else if (status == 403 || status == 500)
                return makeErrorResponse(status, "Index access denied or unavailable");
            else if (!location.getAutoindex())
                return makeErrorResponse(403, "403 Forbidden");
        }
        else if (!location.getAutoindex())
            return makeErrorResponse(403, "403 Forbidden");
        if (S_ISDIR(fileInfo.st_mode) && location.getAutoindex())
        {
            DIR* directory = opendir(RootedPath::descriptorPath(path.targetFd()).c_str());
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

                const std::string entryPath = RootedPath::descriptorPath(path.targetFd()) + "/" + name;
                OwnedFd entryFd(open(entryPath.c_str(), O_PATH | O_NOFOLLOW));
                struct stat entryInfo;
                if (entryFd.get() < 0 || stat(RootedPath::descriptorPath(entryFd.get()).c_str(), &entryInfo) != 0
                    || S_ISLNK(entryInfo.st_mode)) continue;
                const bool isDirectory = S_ISDIR(entryInfo.st_mode);
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

    // Open the pinned inode rather than reopening a replaceable request pathname.
    OwnedFd file(open(RootedPath::descriptorPath(path.targetFd()).c_str(), O_RDONLY | O_NONBLOCK));
    if (file.get() < 0)
        return makeErrorResponse(RootedPath::openErrorStatus(errno), "File access denied or unavailable");
    HTTPResponse response;
    char buffer[8192];
    for (;;)
    {
        // The inspected target is a regular disk file: poll readiness is exempt.
        const ssize_t count = read(file.get(), buffer, sizeof(buffer));
        if (count < 0) return makeErrorResponse(500, "500 Internal Server Error");
        if (count == 0) break;
        response.body.append(buffer, static_cast<std::size_t>(count));
    }
    response.statusCode = 200;
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

    RootedPath path;
    int status = 500;
    if (!path.resolve(location.getUploadDirectory(), relativePath, status))
        return makeErrorResponse(status, "Upload path denied or unavailable");
    const std::string filePath = path.entryPath();
    if (access(RootedPath::descriptorPath(path.parentFd()).c_str(), W_OK) != 0)
        return makeErrorResponse(403, "403 Forbidden");
    // Exclusive creation avoids overwriting an existing file or following a final symlink.
    OwnedFd file(open(filePath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644));
    const int fd = file.get();
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
    if (close(file.release()) != 0) failed = true;
    if (failed)
    {
        std::remove(filePath.c_str());
        return makeErrorResponse(500, "500 Internal Server Error");
    }
    HTTPResponse response;
    response.statusCode = 201;
    response.body = "File uploaded successfully";
    response.headers["Content-Type"] = "text/plain";
    response.headers["Location"] = encodeUriPath(joinPath(location.getPath(), relativePath));
    return response;
}

HTTPResponse RequestResources::handleDelete(const HTTPRequest& request, const Location& location)
{
    if (!isPathSafe(request.uri))
        return makeErrorResponse(403, "403 Forbidden");
    RootedPath path;
    int status = 500;
    struct stat fileInfo;
    if (!path.resolve(location.getRoot(), getRelativePath(request.uri, location), status)
        || !path.inspect(fileInfo, status))
        return makeErrorResponse(status, "Delete path denied or unavailable");

    if (!S_ISREG(fileInfo.st_mode))
        return makeErrorResponse(403, "403 Forbidden");

    // remove never follows the final symlink, and the parent descriptor is pinned.
    if (std::remove(path.entryPath().c_str()) != 0)
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
    for (std::size_t i = 0; i < extension.size(); ++i)
        extension[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));

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
    if (extension == ".pdf")
        return "application/pdf";

    return "application/octet-stream";
}
