#pragma once

#include <string>
#include <sstream>
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
	ParseResult parse(const std::string &raw, HttpRequest &request) const;

private:
	bool parseRequestLine(const std::string &line, HttpRequest &request) const;
	bool parseHeaders(const std::string &headerBlock, HttpRequest &request) const;
	ParseResult parseBody(const std::string &body, HttpRequest &request) const;
};