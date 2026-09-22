#pragma once

#include <string>
#include "HTTPResponse.hpp"

class ResponseBuilder
{
public:
	static std::string serialize(const HTTPResponse& response);
	static HTTPResponse makeError(int statusCode, const std::string& body);
};