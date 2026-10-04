#include "../include/HttpRequest.hpp"

HttpRequest::HttpRequest() : content_length(0), chunked(false) {}

void HttpRequest::setMethod(const std::string &method)
{
	this->method = method;
}

void HttpRequest::setPath(const std::string &path)
{
	this->path = path;
}

void HttpRequest::setQuery(const std::string &query)
{
	this->query = query;
}

void HttpRequest::setVersion(const std::string &version)
{
	this->version = version;
}

void HttpRequest::setHeader(const std::string &name, const std::string value)
{
	headers[name] = value;
}

void HttpRequest::setBody(const std::string &body)
{
	this->body = body;
}

void HttpRequest::setContentLength(size_t length)
{
	content_length = length;
}

void HttpRequest::setChunked(bool value)
{
	chunked = value;
}

const std::string &HttpRequest::getMethod() const
{
	return method;
}

const std::string &HttpRequest::getPath() const
{
	return path;
}

const std::string &HttpRequest::getQuery() const
{
	return query;
}

const std::string &HttpRequest::getVersion() const
{
	return version;
}

const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
	return headers;
}

const std::string &HttpRequest::getBody() const
{
	return body;
}

size_t HttpRequest::getContentLength() const
{
	return content_length;
}

bool HttpRequest::isChunked() const
{
	return chunked;
}