#include "RequestCgi.hpp"
#include <unistd.h>
#include <cctype>
#include <sstream>
#include <limits>
#include <sys/stat.h>

bool RequestCgi::isSupportedMethod(const std::string& method) const
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

bool RequestCgi::isMethodAllowed(const std::string& method, const Location& location) const
{
    for (std::size_t i = 0; i < location.getAllowedMethods().size(); ++i)
    {
        if (location.getAllowedMethods()[i] == method)
            return true;
    }
    return false;
}

std::string RequestCgi::buildAllowHeader(const Location& location) const
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

RequestCgi::CgiResult RequestCgi::prepareCgi(const HTTPRequest& originalRequest,
    const ServerConfig& config, const CgiContext& context, CgiRequest& plan, HTTPResponse& error) const
{
    plan = CgiRequest();
    HTTPRequest request = originalRequest;
    if (!decodeUriPath(originalRequest.uri, request.uri))
    { error = makeErrorResponse(400, "400 Bad Request"); return CGI_ERROR; }
    if (!isPathSafe(request.uri))
    { error = makeErrorResponse(403, "403 Forbidden"); return CGI_ERROR; }
    const Location* location = findLocation(request.uri, config);
    if (location == NULL) return CGI_NOT_SELECTED;
    const std::string& extension = location->getCgiExtension();
    if (extension.empty() || request.uri.size() < extension.size() ||
        request.uri.compare(request.uri.size() - extension.size(), extension.size(), extension) != 0)
        return CGI_NOT_SELECTED;
    if (!isSupportedMethod(request.method))
    { error = makeErrorResponse(501, "501 Not Implemented"); return CGI_ERROR; }
    if (!isMethodAllowed(request.method, *location))
    {
        error = makeErrorResponse(405, "405 Method Not Allowed");
        error.headers["Allow"] = buildAllowHeader(*location);
        return CGI_ERROR;
    }
    if (location->hasRedirect()) return CGI_NOT_SELECTED;
    if (request.body.size() > config.getMaxBodySize())
    { error = makeErrorResponse(413, "413 Payload Too Large"); return CGI_ERROR; }
    if (request.method != "GET" && request.method != "POST")
    { error = makeErrorResponse(405, "405 Method Not Allowed"); error.headers["Allow"] = "GET, POST"; return CGI_ERROR; }
    plan.scriptPath = buildFilePath(request.uri, *location);
    struct stat info;
    if (stat(plan.scriptPath.c_str(), &info) != 0)
    { error = makeErrorResponse(404, "404 Not Found"); return CGI_ERROR; }
    if (!S_ISREG(info.st_mode) || access(plan.scriptPath.c_str(), R_OK) != 0)
    { error = makeErrorResponse(403, "403 Forbidden"); return CGI_ERROR; }
    plan.interpreter = location->getCgiPath();
    if (plan.interpreter.empty() || plan.interpreter[0] != '/' ||
        access(plan.interpreter.c_str(), X_OK) != 0 || context.serverPort < 1 || context.serverPort > 65535)
    { error = makeErrorResponse(500, "500 Internal Server Error"); return CGI_ERROR; }
    plan.body = request.body;
    std::ostringstream length;
    length << request.body.size();
    std::ostringstream port;
    port << context.serverPort;
    plan.environment.push_back("GATEWAY_INTERFACE=CGI/1.1");
    plan.environment.push_back("SERVER_SOFTWARE=webserv");
    plan.environment.push_back("SERVER_NAME=" + (config.getServerName().empty() ? config.getHost() : config.getServerName()));
    plan.environment.push_back("SERVER_PORT=" + port.str());
    plan.environment.push_back("SERVER_PROTOCOL=" + context.protocol);
    plan.environment.push_back("REQUEST_METHOD=" + request.method);
    plan.environment.push_back("SCRIPT_NAME=" + originalRequest.uri);
    plan.environment.push_back("PATH_INFO=");
    plan.environment.push_back("QUERY_STRING=" + context.query);
    plan.environment.push_back("REMOTE_ADDR=" + context.remoteAddress);
    plan.environment.push_back("CONTENT_LENGTH=" + length.str());
    std::map<std::string, std::string>::const_iterator contentType = request.headers.find("content-type");
    plan.environment.push_back("CONTENT_TYPE=" + (contentType == request.headers.end() ? "" : contentType->second));
    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
         it != request.headers.end(); ++it)
    {
        if (it->first == "content-type" || it->first == "content-length" ||
            it->first == "transfer-encoding" || it->first == "connection" || it->first == "proxy")
            continue;
        std::string key = "HTTP_";
        for (std::size_t i = 0; i < it->first.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(it->first[i]);
            key += c == '-' ? '_' : static_cast<char>(std::toupper(c));
        }
        plan.environment.push_back(key + "=" + it->second);
    }
    return CGI_READY;
}


HTTPResponse RequestCgi::parseCgiOutput(const std::string& output) const
{
    const HTTPResponse invalid = makeErrorResponse(500, "500 Invalid CGI Response");
    std::size_t end = output.find("\r\n\r\n");
    std::size_t separator = 4;
    const std::size_t lfEnd = output.find("\n\n");
    if (lfEnd != std::string::npos && (end == std::string::npos || lfEnd < end))
    { end = lfEnd; separator = 2; }
    if (end == std::string::npos || end > 16384) return invalid;
    HTTPResponse response;
    response.body = output.substr(end + separator);
    std::map<std::string, std::string> fields;
    std::istringstream headers(output.substr(0, end));
    std::string line;
    bool explicitStatus = false;
    while (std::getline(headers, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0) return invalid;
        std::string name = line.substr(0, colon);
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(name[i]);
            if (!std::isalnum(c) && std::string("!#$%&'*+-.^_`|~").find(c) == std::string::npos)
                return invalid;
            name[i] = static_cast<char>(std::tolower(c));
        }
        std::string value = line.substr(colon + 1);
        const std::size_t first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? "" : value.substr(first);
        const std::size_t last = value.find_last_not_of(" \t");
        if (last != std::string::npos) value.erase(last + 1);
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if ((c < 32 && c != '\t') || c == 127) return invalid;
        }
        // HTTPResponse stores one value per header: reject duplicates instead of losing them.
        if (fields.count(name)) return invalid;
        fields[name] = value;
    }
    if (fields.count("status"))
    {
        const std::string& status = fields["status"];
        if (status.size() < 3 || (status.size() > 3 && status[3] != ' ')) return invalid;
        for (std::size_t i = 0; i < 3; ++i)
            if (status[i] < '0' || status[i] > '9') return invalid;
        response.statusCode = (status[0] - '0') * 100 + (status[1] - '0') * 10 + status[2] - '0';
        if (response.statusCode < 200 || response.statusCode > 599) return invalid;
        explicitStatus = true;
    }
    if (!explicitStatus && fields.count("location")) response.statusCode = 302;
    if (fields.count("content-length"))
    {
        const std::string& length = fields["content-length"];
        if (length.empty()) return invalid;
        std::size_t count = 0;
        for (std::size_t i = 0; i < length.size(); ++i)
        {
            if (length[i] < '0' || length[i] > '9') return invalid;
            const std::size_t digit = length[i] - '0';
            if (count > (std::numeric_limits<std::size_t>::max() - digit) / 10) return invalid;
            count = count * 10 + digit;
        }
        if (count != response.body.size()) return invalid;
    }
    // CGI stdout is an unencoded entity body. Transfer coding is not supported here.
    if (fields.count("transfer-encoding")) return invalid;
    if (fields.count("connection"))
    {
        std::string tokens = fields["connection"];
        for (std::size_t i = 0; i < tokens.size(); ++i)
            tokens[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(tokens[i])));
        std::istringstream list(tokens);
        std::string token;
        while (std::getline(list, token, ','))
        {
            const std::size_t first = token.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            token = token.substr(first);
            token.erase(token.find_last_not_of(" \t") + 1);
            // Do not let Connection change interpretation of CGI control fields.
            if (token == "status" || token == "content-length" || token == "content-type") return invalid;
            fields.erase(token);
        }
    }
    for (std::map<std::string, std::string>::const_iterator it = fields.begin(); it != fields.end(); ++it)
    {
        if (it->first == "status" || it->first == "content-length" || it->first == "connection" ||
            it->first == "keep-alive" || it->first == "proxy-connection" || it->first == "te" ||
            it->first == "trailer" || it->first == "upgrade") continue;
        std::string name = it->first;
        bool capitalize = true;
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            if (capitalize) name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
            capitalize = name[i] == '-';
        }
        response.headers[name] = it->second;
    }
    if (response.statusCode == 204 || response.statusCode == 304) response.body.clear();
    if (!response.headers.count("Content-Type") && !response.headers.count("Location") &&
        response.statusCode != 204 && response.statusCode != 304) return invalid;
    std::ostringstream length;
    length << response.body.size();
    response.headers["Content-Length"] = length.str();
    return response;
}
