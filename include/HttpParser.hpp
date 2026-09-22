#pragma once

#include <string>
#include "HttpRequest.hpp"

class HttpParser
{
public:
	enum ParseResult
	{
		INCOMPLETE,
		COMPLETE,
		ERROR
	};
	ParseResult parse(const std::string &raw, HTTPRequest &request) const;

private:
	bool parseRequestLine(const std::string &line, HTTPRequest &request) const;
	bool parseHeaders(const std::string &headerBlock, HTTPRequest &request) const;
	bool parseBody(const std::string &body, HTTPRequest &request) const;
};