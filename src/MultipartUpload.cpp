#include "MultipartUpload.hpp"
#include <cctype>

std::string MultipartUpload::trim(const std::string& value)
{
    const std::size_t start = value.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    return value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

std::string MultipartUpload::lower(std::string value)
{
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return value;
}

bool MultipartUpload::parameters(const std::string& text, std::string& kind,
                       std::map<std::string, std::string>& values)
{
    values.clear();
    const std::size_t separator = text.find(';');
    kind = lower(trim(text.substr(0, separator)));
    std::size_t pos = separator;
    while (pos != std::string::npos && pos < text.size())
    {
        ++pos;
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
        {
            ++pos;
        }
        const std::size_t equals = text.find('=', pos);
        const std::size_t next = text.find(';', pos);
        if (equals == std::string::npos || (next != std::string::npos && next < equals))
            return false;
        const std::string key = lower(trim(text.substr(pos, equals - pos)));
        if (key.empty() || values.count(key)) 
            return false;
        pos = equals + 1;
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
        {
            ++pos;
        }
        std::string value;
        if (pos < text.size() && text[pos] == '"')
        {
            ++pos;
            bool closed = false;
            while (pos < text.size())
            {
                char c = text[pos++];
                if (c == '"')
                {
                    closed = true;
                    break;
                }
                if (c == '\\')
                {
                    if (pos == text.size())
                        return false;
                    c = text[pos++];
                }
                value += c;
            }
            if (!closed)
                return false;
            while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
            {
                ++pos;
            }
            if (pos < text.size() && text[pos] != ';')
                return false;
        }
        else
        {
            const std::size_t end = text.find(';', pos);
            value = trim(text.substr(pos, end == std::string::npos ? end : end - pos));
            pos = end == std::string::npos ? text.size() : end;
        }
        if (value.find('\r') != std::string::npos || value.find('\n') != std::string::npos)
            return false;
        values[key] = value;
    }
    return !kind.empty();
}

bool MultipartUpload::parse(const std::string& contentType, const std::string& data)
{
    filename.clear(); body.clear();
    std::string kind;
    std::map<std::string, std::string> params;
    if (!parameters(contentType, kind, params) || kind != "multipart/form-data")
        return false;
    const std::string boundary = params["boundary"];
    if (boundary.empty() || boundary.size() > 70 || boundary[boundary.size() - 1] == ' ')
        return false;
    for (std::size_t i = 0; i < boundary.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(boundary[i]);
        if (!(std::isalnum(c) || std::string("'()+_,-./:=? ").find(c) != std::string::npos))
            return false;
    }
    const std::string marker = "--" + boundary;
    const std::string delimiter = "\r\n" + marker;
    if (data.compare(0, marker.size() + 2, marker + "\r\n") != 0)
        return false;
    std::size_t pos = marker.size() + 2;
    bool found = false;
    for (;;)
    {
        const std::size_t endHeaders = data.find("\r\n\r\n", pos);
        if (endHeaders == std::string::npos || endHeaders - pos > 8192)
            return false;
        std::map<std::string, std::string> headers;
        std::size_t lineStart = pos;
        while (lineStart < endHeaders)
        {
            std::size_t lineEnd = data.find("\r\n", lineStart);
            if (lineEnd == std::string::npos || lineEnd > endHeaders)
                return false;
            const std::size_t colon = data.find(':', lineStart);
            if (colon == std::string::npos || colon >= lineEnd)
                return false;
            const std::string name = lower(trim(data.substr(lineStart, colon - lineStart)));
            if (name.empty() || headers.count(name))
                return false;
            headers[name] = trim(data.substr(colon + 1, lineEnd - colon - 1));
            lineStart = lineEnd + 2;
        }
        std::map<std::string, std::string>::const_iterator disposition = headers.find("content-disposition");
        if (disposition == headers.end() || !parameters(disposition->second, kind, params) || kind != "form-data" || !params.count("name"))
                return false;
        const std::size_t startBody = endHeaders + 4;
        std::size_t endBody = data.find(delimiter, startBody);
        while (endBody != std::string::npos)
        {
            const std::size_t suffix = endBody + delimiter.size();
            if (data.compare(suffix, 2, "\r\n") == 0)
                break;
            if (data.compare(suffix, 2, "--") == 0 && (suffix + 2 == data.size() || data.compare(suffix + 2, 2, "\r\n") == 0))
                break;
            endBody = data.find(delimiter, endBody + 1);
        }
        if (endBody == std::string::npos)
            return false;
        if (params.count("filename"))
        {
            if (found || params["filename"].empty())
                return false;
            filename = params["filename"];
            if (filename == "." || filename == ".." || filename.find_first_of("/\\") != std::string::npos)
                    return false;
            for (std::size_t i = 0; i < filename.size(); ++i)
            {
                const unsigned char c = static_cast<unsigned char>(filename[i]);
                if (c < 32 || c == 127)
                    return false;
            }
            body = data.substr(startBody, endBody - startBody);
            found = true;
        }
        pos = endBody + delimiter.size();
        if (data.compare(pos, 2, "--") == 0)
            return found && (pos + 2 == data.size() || data.compare(pos + 2, 2, "\r\n") == 0);
        pos += 2;
    }
}
