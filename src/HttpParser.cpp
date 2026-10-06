#include "../include/HttpParser.hpp"
#include <limits>
#include <cctype>
#include <vector>

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
			if (!std::isalnum(c) && std::string("!#$%&'*+-.^_`|~").find(c) == std::string::npos)
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
				if (maxBodySize != 0 && decoded.size() > maxBodySize)
					return TOO_LARGE;
				return COMPLETE;
			}
			if (maxBodySize != 0 && decoded.size() + count > maxBodySize)
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
	if (maxBodySize != 0 && count > maxBodySize)
		return TOO_LARGE;
	if (body.size() < count)
		return INCOMPLETE;
	request.setBody(body.substr(0, count));
	return COMPLETE;
}
