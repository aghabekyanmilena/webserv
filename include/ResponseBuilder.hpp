#pragma once

#include <string>
#include "HttpResponse.hpp"

class ResponseBuilder
{
public:
	static std::string serialize(const HttpResponse& response);
	static HttpResponse makeError(int statusCode, const std::string& body);
};