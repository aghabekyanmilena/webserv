#pragma once

#include <string>
#include <map>
#include <iostream>

class HttpResponse
{
private:
	int status_code;
	std::string reason_phrase;
	std::map<std::string, std::string> headers;
	std::string body;
public:
	HttpResponse();

	void setStatusCode(int code);
	void setReasonPhrase(const std::string &reason);
	void setHeader(const std::string &name, const std::string &value);
	void setBody(const std::string &body);

	int getStatusCode() const;
	const std::string &getReasonPhrase() const;
	const std::map<std::string, std::string> &getHeaders() const;
	const std::string &getBody() const;
};