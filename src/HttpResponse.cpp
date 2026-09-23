#include "../include/HttpResponse.hpp"

HttpResponse::HttpResponse() : status_code(200), reason_phrase("OK")
{}

void HttpResponse::setStatusCode(int code)
{
	this->status_code = code;
}

void HttpResponse::setReasonPhrase(const std::string& reason)
{
	this->reason_phrase = reason;
}

void HttpResponse::setHeader(const std::string& name, const std::string& value)
{
	headers[name] = value;
}

void HttpResponse::setBody(const std::string& body)
{
	this->body = body;
}

int HttpResponse::getStatusCode() const
{
	return status_code;
}

const std::string& HttpResponse::getReasonPhrase() const
{
	return reason_phrase;
}

const std::map<std::string, std::string> &HttpResponse::getHeaders() const
{
	return headers;
}

const std::string& HttpResponse::getBody() const
{
	return body;
}