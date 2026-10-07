#include "RequestUtils.hpp"

std::string RequestUtils::escapeHtml(const std::string& text) const
{
    std::string escaped;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        switch (text[i])
        {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += text[i]; break;
        }
    }
    return escaped;
}

std::string RequestUtils::encodePathSegment(const std::string& name) const
{
    const char* hex = "0123456789ABCDEF";
    std::string encoded;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~')
            encoded += static_cast<char>(c);
        else
        {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 15];
        }
    }
    return "./" + encoded;
}

bool RequestUtils::decodeUriPath(const std::string& encoded, std::string& decoded) const
{
    const std::string digits = "0123456789abcdef";
    decoded.clear();
    for (std::size_t i = 0; i < encoded.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(encoded[i]);
        if (c == '%')
        {
            if (encoded.size() - i < 3)
                return false;
            char high = encoded[i + 1];
            char low = encoded[i + 2];
            if (high >= 'A' && high <= 'F')
                high = static_cast<char>(high - 'A' + 'a');
            if (low >= 'A' && low <= 'F')
                low = static_cast<char>(low - 'A' + 'a');
            const std::size_t highValue = digits.find(high);
            const std::size_t lowValue = digits.find(low);
            if (highValue == std::string::npos || lowValue == std::string::npos)
                return false;
            c = static_cast<unsigned char>(highValue * 16 + lowValue);
            i += 2;
        }
        if (c == 0 || c < 32 || c == 127)
            return false;
        decoded += static_cast<char>(c);
    }
    return !decoded.empty() && decoded[0] == '/';
}

bool RequestUtils::isPathSafe(const std::string& path) const
{
    if (path.empty() || path[0] != '/')
        return false;
    std::size_t start = 1;
    while (start <= path.size())
    {
        const std::size_t end = path.find('/', start);
        if (path.substr(start, end == std::string::npos ? end : end - start) == "..")
            return false;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return true;
}

std::string RequestUtils::encodeUriPath(const std::string& path) const
{
    std::string encoded;
    for (std::size_t i = 0; i < path.size(); ++i)
        encoded += path[i] == '/' ? "/" : encodePathSegment(std::string(1, path[i])).substr(2);
    return encoded;
}

HTTPResponse RequestUtils::makeErrorResponse(int statusCode, const std::string& message) const
{
    HTTPResponse response;

    response.statusCode = statusCode;
    response.body = "<html><body><h1>" + message + "</h1></body></html>";
    response.headers["Content-Type"] = "text/html";
    return response;
}
