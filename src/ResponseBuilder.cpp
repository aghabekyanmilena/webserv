#include "../include/ResponseBuilder.hpp"
#include <sstream>

std::string ResponseBuilder::serialize(const HTTPResponse& response)
{
	std::ostringstream output;

	output << "HTTP/1.1 "
		<< response.getStatusCode()
		<< " "
		<< response.getReasonPhrase()
		<< "\r\n";

	const std::map<std::string, std::string>& headers = response.getHeaders();
	std::map<std::string, std::string>::const_iterator it;

	for (it = headers.begin(); it != headers.end(); ++it)
	{
		output << it->first
				<< ": "
				<< it->second
				<< "\r\n";
	}

	output << "\r\n";
	output << response.getBody();

	return output.str();
}

HTTPResponse ResponseBuilder::makeError(int statusCode, const std::string& body)
{
	HTTPResponse response;

	response.setStatusCode(statusCode);
	response.setReasonPhrase("Error");
	response.setBody(body);

	return response;
}