#include "../include/HttpParser.hpp"
#include <sstream>

HttpParser::ParseResult
HttpParser::parse(const std::string& raw, HTTPRequest& request) const
{
	std::size_t headerEnd = raw.find("\r\n\r\n");

	if (headerEnd == std::string::npos)
		return INCOMPLETE;

	std::string headerPart = raw.substr(0, headerEnd);
	std::size_t firstLineEnd = headerPart.find("\r\n");

	if (firstLineEnd == std::string::npos)
		return ERROR;

	std::string requestLine = headerPart.substr(0, firstLineEnd);

	if (!parseRequestLine(requestLine, request))
		return ERROR;

	std::string headers =
		headerPart.substr(firstLineEnd + 2);

	if (!parseHeaders(headers, request))
		return ERROR;

	std::string body =
		raw.substr(headerEnd + 4);

	ParseResult bodyResult = parseBody(body, request);

	if (bodyResult != COMPLETE)
		return bodyResult;

	return COMPLETE;
}

bool HttpParser::parseRequestLine(const std::string& line, HTTPRequest& request) const
{
	std::stringstream ss(line);
	std::string method;
	std::string target;
	std::string version;
	ss >> method >> target >> version;

	if (method.empty() || target.empty() || version.empty())
		return false;

	request.setMethod(method);
	request.setVersion(version);

	std::size_t question = target.find('?');

	if (question == std::string::npos)
	{
		request.setPath(target);
		request.setQuery("");
	}
	else
	{
		request.setPath(target.substr(0, question));
		request.setQuery(target.substr(question + 1));
	}

	if (version != "HTTP/1.0" && version != "HTTP/1.1")
		return false;

	return true;
}

bool HttpParser::parseHeaders(const std::string& headerBlock, HTTPRequest& request) const
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
		std::size_t start = value.find_first_not_of(" \t");

		if (start != std::string::npos)
			value = value.substr(start);
		else
			value = "";
		request.setHeader(name, value);
	}
	return true;
}

HttpParser::ParseResult
HttpParser::parseBody(const std::string& body, HTTPRequest& request) const
{
	const std::map<std::string, std::string>& headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("Content-Length");

	if (it == headers.end())
	{
		request.setBody("");
		return COMPLETE;
	}

	std::stringstream ss(it->second);
	std::size_t contentLength;
	ss >> contentLength;

	if (ss.fail())
		return ERROR;

	request.setContentLength(contentLength);

	if (body.size() < contentLength)
		return INCOMPLETE;

	request.setBody(body.substr(0, contentLength));
	return COMPLETE;
}