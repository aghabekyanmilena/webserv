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
		ERROR,
		HEADER_TOO_LARGE,
		TOO_LARGE
	};

	static const std::size_t MAX_HEADER_SIZE = 8192;

	ParseResult parse(const std::string &raw, HttpRequest &request,
					  std::size_t maxBodySize = 0) const;

private:
	bool parseRequestLine(const std::string &line, HttpRequest &request) const;
	bool parseHeaders(const std::string &headerBlock, HttpRequest &request) const;
	ParseResult parseBody(const std::string &body, HttpRequest &request,
						  std::size_t maxBodySize) const;
	bool decodePath(const std::string &rawPath, std::string &decoded) const;
	bool normalizePath(const std::string &path, std::string &normalized) const;
	static int hexValue(char c);
};
