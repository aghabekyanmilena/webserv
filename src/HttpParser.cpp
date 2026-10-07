#include "../include/HttpParser.hpp"
#include <limits>
#include <cctype>
#include <vector>

bool HttpParser::tokenChar(unsigned char c)
{
    return (c < 128 && std::isalnum(c)) ||
        std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
}

bool HttpParser::regNameChar(unsigned char c)
{
    return (c < 128 && std::isalnum(c)) ||
        std::string("-._~!$&'()*+,;=").find(c) != std::string::npos;
}

bool HttpParser::ipv4(const std::string& text)
{
    std::size_t start = 0;
    int parts = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find('.', start);
        const std::string part = text.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part.size() > 3) return false;
        unsigned int value = 0;
        for (std::size_t i = 0; i < part.size(); ++i)
        {
            if (part[i] < '0' || part[i] > '9') return false;
            value = value * 10 + part[i] - '0';
        }
        if (value > 255 || ++parts > 4) return false;
        if (end == std::string::npos) return parts == 4;
        start = end + 1;
    }
    return false;
}

bool HttpParser::ipLiteral(const std::string& text)
{
    if (text.empty()) return false;
    if (text[0] == 'v' || text[0] == 'V')
    {
        const std::size_t dot = text.find('.');
        if (dot == std::string::npos || dot < 2 || dot + 1 == text.size()) return false;
        for (std::size_t i = 1; i < dot; ++i)
            if (!std::isxdigit(static_cast<unsigned char>(text[i]))) return false;
        for (std::size_t i = dot + 1; i < text.size(); ++i)
            if (!regNameChar(text[i]) && text[i] != ':') return false;
        return true;
    }
    const std::size_t compression = text.find("::");
    if (text.find(":::") != std::string::npos ||
        (compression != std::string::npos && text.find("::", compression + 2) != std::string::npos) ||
        (text[0] == ':' && text.compare(0, 2, "::") != 0) ||
        (text[text.size() - 1] == ':' && (text.size() < 2 || text.compare(text.size() - 2, 2, "::") != 0)))
        return false;
    unsigned int groups = 0;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find(':', start);
        const std::string part = text.substr(start, end == std::string::npos ? end : end - start);
        if (!part.empty())
        {
            if (part.find('.') != std::string::npos)
            {
                if (end != std::string::npos || !ipv4(part)) return false;
                groups += 2;
            }
            else
            {
                if (part.size() > 4) return false;
                for (std::size_t i = 0; i < part.size(); ++i)
                    if (!std::isxdigit(static_cast<unsigned char>(part[i]))) return false;
                ++groups;
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return compression == std::string::npos ? groups == 8 : groups < 8;
}

bool HttpParser::validHost(const std::string& value)
{
    std::size_t port = std::string::npos;
    if (!value.empty() && value[0] == '[')
    {
        const std::size_t end = value.find(']');
        if (end == std::string::npos || !ipLiteral(value.substr(1, end - 1))) return false;
        if (end + 1 < value.size())
        {
            if (value[end + 1] != ':') return false;
            port = end + 2;
        }
    }
    else
    {
        const std::size_t colon = value.find(':');
        const std::size_t end = colon == std::string::npos ? value.size() : colon;
        for (std::size_t i = 0; i < end; ++i)
        {
            const unsigned char c = value[i];
            if (c == '%')
            {
                if (end - i < 3 || !std::isxdigit(static_cast<unsigned char>(value[i + 1])) ||
                    !std::isxdigit(static_cast<unsigned char>(value[i + 2]))) return false;
                i += 2;
            }
            else if (!regNameChar(c)) return false;
        }
        if (colon != std::string::npos) port = colon + 1;
    }
    if (port != std::string::npos)
        for (std::size_t i = port; i < value.size(); ++i)
            if (value[i] < '0' || value[i] > '9') return false;
    return true;
}
int HttpParser::hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool HttpParser::decodePath(const std::string &rawPath, std::string &decoded) const
{
    decoded.clear();
    decoded.reserve(rawPath.size());
    std::size_t i = 0;
    while (i < rawPath.size())
    {
        const char c = rawPath[i];
        if (c == '%')
        {
            if (i + 2 >= rawPath.size())
                return false;
            const int high = hexValue(rawPath[i + 1]);
            const int low = hexValue(rawPath[i + 2]);
            if (high < 0 || low < 0)
                return false;
            const char value = static_cast<char>((high << 4) | low);
            if (value == '\0')
                return false;
            decoded.push_back(value);
            i += 3;
        }
        else
        {
            decoded.push_back(c);
            ++i;
        }
    }
    return true;
}

bool HttpParser::normalizePath(const std::string &path, std::string &normalized) const
{
    if (path.empty() || path[0] != '/')
        return false;

    std::vector<std::string> segments;
    std::size_t i = 0;
    while (i < path.size())
    {
        while (i < path.size() && path[i] == '/')
            ++i;
        if (i >= path.size())
            break;
        const std::size_t start = i;
        while (i < path.size() && path[i] != '/')
            ++i;
        const std::string segment = path.substr(start, i - start);
        if (segment == ".")
            continue;
        if (segment == "..")
        {
            if (segments.empty())
                return false;
            segments.pop_back();
            continue;
        }
        segments.push_back(segment);
    }

    normalized = "/";
    for (std::size_t n = 0; n < segments.size(); ++n)
    {
        if (n != 0)
            normalized += "/";
        normalized += segments[n];
    }
    if (path.size() > 1 && path[path.size() - 1] == '/' && normalized != "/")
        normalized += "/";
    return true;
}

HttpParser::ParseResult
HttpParser::parse(const std::string &raw, HttpRequest &request, std::size_t maxBodySize) const
{
    request = HttpRequest();
    std::size_t headerEnd = raw.find("\r\n\r\n");

    if (headerEnd == std::string::npos)
    {
        if (raw.size() > MAX_HEADER_SIZE)
            return HEADER_TOO_LARGE;
        return INCOMPLETE;
    }
    if (headerEnd > MAX_HEADER_SIZE)
        return HEADER_TOO_LARGE;

    std::string headerPart = raw.substr(0, headerEnd);
    std::size_t firstLineEnd = headerPart.find("\r\n");

    if (firstLineEnd == std::string::npos)
        firstLineEnd = headerPart.size();

    std::string requestLine = headerPart.substr(0, firstLineEnd);

    if (!parseRequestLine(requestLine, request))
        return ERROR;

    std::string headers =
        firstLineEnd == headerPart.size() ? "" : headerPart.substr(firstLineEnd + 2);

    if (!parseHeaders(headers, request))
        return ERROR;

    if (request.getVersion() == "HTTP/1.1"
        && request.getHeaders().find("host") == request.getHeaders().end())
        return ERROR;

    std::string body = raw.substr(headerEnd + 4);
    ParseResult bodyResult = parseBody(body, request, maxBodySize);

    if (bodyResult != COMPLETE)
        return bodyResult;

    return COMPLETE;
}

bool HttpParser::parseRequestLine(const std::string &line, HttpRequest &request) const
{
    std::stringstream ss(line);
    std::string method;
    std::string target;
    std::string version;
    ss >> method >> target >> version;

    std::string extra;
    if (method.empty() || target.empty() || target[0] != '/'
        || version.empty() || (ss >> extra))
        return false;

    request.setMethod(method);
    request.setVersion(version);
    for (std::size_t i = 0; i < method.size(); ++i)
        if (!tokenChar(static_cast<unsigned char>(method[i]))) return false;
    for (std::size_t i = 0; i < line.size(); ++i)
        if (static_cast<unsigned char>(line[i]) < 32 || line[i] == 127) return false;

    std::size_t question = target.find('?');
    std::string rawPath;
    if (question == std::string::npos)
    {
        rawPath = target;
        request.setQuery("");
    }
    else
    {
        rawPath = target.substr(0, question);
        request.setQuery(target.substr(question + 1));
    }

    std::string decoded;
    if (!decodePath(rawPath, decoded))
        return false;
    std::string normalized;
    if (!normalizePath(decoded, normalized))
        return false;
    request.setPath(normalized);

    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        return false;

    return true;
}

bool HttpParser::parseHeaders(const std::string &headerBlock, HttpRequest &request) const
{
    for (std::size_t i = 0; i < headerBlock.size(); ++i)
        if ((headerBlock[i] == '\n' && (i == 0 || headerBlock[i - 1] != '\r')) ||
            (headerBlock[i] == '\r' && (i + 1 == headerBlock.size() || headerBlock[i + 1] != '\n')))
            return false;
    std::stringstream ss(headerBlock);
    std::string line;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            continue;

        std::size_t colon = line.find(':');

        if (colon == std::string::npos)
            return false;

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        if (name.empty())
            return false;
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(name[i]);
            if (!tokenChar(c))
                return false;
            name[i] = static_cast<char>(std::tolower(c));
        }
        if (request.getHeaders().count(name))
            return false;
        std::size_t start = value.find_first_not_of(" \t");

        if (start != std::string::npos)
            value = value.substr(start);
        else
            value = "";
        std::size_t end = value.find_last_not_of(" \t");
        if (end != std::string::npos)
            value.erase(end + 1);
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = value[i];
            if ((c < 32 && c != '\t') || c == 127) return false;
        }
        if (name == "host" && !validHost(value)) return false;
        request.setHeader(name, value);
    }
    return true;
}

HttpParser::ParseResult
HttpParser::parseBody(const std::string &body, HttpRequest &request,
                      std::size_t maxBodySize) const
{
    const std::map<std::string, std::string> &headers = request.getHeaders();
    std::map<std::string, std::string>::const_iterator length = headers.find("content-length");
    std::map<std::string, std::string>::const_iterator transfer = headers.find("transfer-encoding");
    if (transfer != headers.end())
    {
        if (length != headers.end())
            return ERROR;
        std::string encoding = transfer->second;
        for (std::size_t i = 0; i < encoding.size(); ++i)
            encoding[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(encoding[i])));
        if (encoding != "chunked")
            return ERROR;
        request.setChunked(true);
        std::size_t pos = 0;
        std::string decoded;
        for (;;)
        {
            std::size_t end = body.find("\r\n", pos);
            if (end == std::string::npos)
            {
                request.setBody(decoded);
                return INCOMPLETE;
            }
            std::string size = body.substr(pos, end - pos);
            std::size_t extension = size.find(';');
            if (extension != std::string::npos)
                size.erase(extension);
            if (size.empty())
                return ERROR;
            std::size_t count = 0;
            for (std::size_t i = 0; i < size.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(size[i]);
                if (!std::isxdigit(c))
                    return ERROR;
                std::size_t digit = c <= '9' ? c - '0' : std::tolower(c) - 'a' + 10;
                if (count > (std::numeric_limits<std::size_t>::max() - digit) / 16)
                    return ERROR;
                count = count * 16 + digit;
            }
            pos = end + 2;
            if (count == 0)
            {
                if (body.size() - pos < 2)
                {
                    request.setBody(decoded);
                    return INCOMPLETE;
                }
                if (body.compare(pos, 2, "\r\n") != 0)
                {
                    if (body.find("\r\n\r\n", pos) == std::string::npos)
                    {
                        request.setBody(decoded);
                        return INCOMPLETE;
                    }
                }
                request.setBody(decoded);
                if (decoded.size() > maxBodySize)
                    return TOO_LARGE;
                return COMPLETE;
            }
            if (count > maxBodySize - decoded.size())
            {
                request.setContentLength(decoded.size() + count);
                request.setBody(decoded);
                return TOO_LARGE;
            }
            if (count > body.size() - pos || body.size() - pos - count < 2)
            {
                request.setBody(decoded);
                return INCOMPLETE;
            }
            if (body.compare(pos + count, 2, "\r\n") != 0)
                return ERROR;
            decoded.append(body, pos, count);
            pos += count + 2;
        }
    }
    if (length == headers.end())
    {
        request.setBody("");
        return COMPLETE;
    }
    if (length->second.empty())
        return ERROR;
    std::size_t count = 0;
    for (std::size_t i = 0; i < length->second.size(); ++i)
    {
        char c = length->second[i];
        if (c < '0' || c > '9')
            return ERROR;
        std::size_t digit = c - '0';
        if (count > (std::numeric_limits<std::size_t>::max() - digit) / 10)
            return ERROR;
        count = count * 10 + digit;
    }
    request.setContentLength(count);
    if (count > maxBodySize)
        return TOO_LARGE;
    if (body.size() < count)
        return INCOMPLETE;
    request.setBody(body.substr(0, count));
    return COMPLETE;
}
