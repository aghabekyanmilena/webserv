#include "../include/HttpRequest.hpp"

HttpRequest::HttpRequest() : content_length(0), chunked(false) {}

void HttpRequest::setMethod(const std::string &method)
{
	method = method;
}

void HttpRequest::setPath(const std::string &path)
{
	path = path;
}

void HttpRequest::setQuery(const std::string &query)
{
	query = query;
}

void HttpRequest::setVersion(const std::string &version)
{
	version = version;
}

void HttpRequest::setHeader(const std::string &name, const std::string value)
{
	headers[name] = value;
}

void HttpRequest::setBody(const std::string &body)
{
	body = body;
}

void HttpRequest::setContentLength(size_t length)
{
	content_length = length;
}

void HttpRequest::setChunked(bool value)
{
	chunked = value;
}

// getters