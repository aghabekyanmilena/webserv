#include "../include/HttpResponse.hpp"

HTTPResponse::~HTTPResponse() {}

void HTTPResponse::setStatusCode(int code)
{
	status_code = code;
}

void HTTPResponse::setReasonPhrase(const std::string& reason)
{
	reason_phrase = reason;
}

void HTTPResponse::setHeader(const std::string& name, const std::string& value)
{
	headers[name] = value;
}

void HTTPResponse::setBody(const std::string& body)
{
	body = body;
}

int HTTPResponse::getStatusCode() const
{
	return status_code;
}

const std::string& HTTPResponse::getReasonPhrase() const
{
	return reason_phrase;
}

const std::map<std::string, std::string> &HTTPResponse::getHeaders() const
{
	return headers;
}

const std::string& HTTPResponse::getBody() const
{
	return body;
}