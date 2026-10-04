#include "../include/ResponseBuilder.hpp"
#include <sstream>

std::string ResponseBuilder::serialize(const HttpResponse& response)
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

HttpResponse ResponseBuilder::makeError(int statusCode, const std::string& body)
{
	HttpResponse response;

	response.setStatusCode(statusCode);
	switch (statusCode)
    {
        case 200: response.setReasonPhrase("OK"); break;
        case 201: response.setReasonPhrase("Created"); break;
        case 204: response.setReasonPhrase("No Content"); break;
        case 301: response.setReasonPhrase("Moved Permanently"); break;
        case 302: response.setReasonPhrase("Found"); break;
        case 303: response.setReasonPhrase("See Other"); break;
        case 307: response.setReasonPhrase("Temporary Redirect"); break;
        case 308: response.setReasonPhrase("Permanent Redirect"); break;
        case 400: response.setReasonPhrase("Bad Request"); break;
        case 403: response.setReasonPhrase("Forbidden"); break;
        case 404: response.setReasonPhrase("Not Found"); break;
        case 405: response.setReasonPhrase("Method Not Allowed"); break;
        case 413: response.setReasonPhrase("Payload Too Large"); break;
        case 500: response.setReasonPhrase("Internal Server Error"); break;
        case 501: response.setReasonPhrase("Not Implemented"); break;
        default: response.setReasonPhrase("Unknown"); break;
    }
	response.setBody(body);

	return response;
}