#include "../include/ResponseBuilder.hpp"
#include <sstream>
#include <cctype>
#include <map>

std::string ResponseBuilder::reasonPhrase(int statusCode)
{
	switch (statusCode)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 504: return "Gateway Timeout";
		default: return "Unknown";
	}
}

std::string ResponseBuilder::defaultErrorBody(int statusCode)
{
	const std::string phrase = reasonPhrase(statusCode);
	std::ostringstream body;
	body << "<!DOCTYPE html>\n"
		 << "<html><head><title>" << statusCode << " " << phrase << "</title></head>"
		 << "<body><h1>" << statusCode << " " << phrase << "</h1>"
		 << "<p>webserv default error page</p></body></html>\n";
	return body.str();
}

std::string ResponseBuilder::serialize(const HttpResponse &response)
{
	std::ostringstream output;

	output << "HTTP/1.1 "
		   << response.getStatusCode()
		   << " "
		   << response.getReasonPhrase()
		   << "\r\n";

	const std::map<std::string, std::string> &headers = response.getHeaders();
	std::map<std::string, std::string>::const_iterator it;
	bool hasContentLength = false;

	for (it = headers.begin(); it != headers.end(); ++it)
	{
		std::string name = it->first;
		for (std::size_t i = 0; i < name.size(); ++i)
			name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
		if (name == "content-length")
		{
			hasContentLength = true;
			continue;
		}
		output << it->first
			   << ": "
			   << it->second
			   << "\r\n";
	}

	(void)hasContentLength;
	output << "Content-Length: " << response.getBody().size() << "\r\n";
	output << "\r\n";
	output << response.getBody();

	return output.str();
}

HttpResponse ResponseBuilder::makeError(int statusCode, const std::string &body)
{
	HttpResponse response;

	response.setStatusCode(statusCode);
	response.setReasonPhrase(reasonPhrase(statusCode));
	if (body.empty() && statusCode >= 400)
	{
		response.setBody(defaultErrorBody(statusCode));
		response.setHeader("Content-Type", "text/html");
	}
	else
		response.setBody(body);

	return response;
}
