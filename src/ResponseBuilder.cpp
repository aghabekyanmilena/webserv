#include "../include/ResponseBuilder.hpp"
#include <sstream>
#include <cctype>
#include <map>
#include <fstream>
#include <ctime>

static std::string httpDate()
{
	char buffer[64];
	std::time_t now = std::time(NULL);
	std::tm *gmt = std::gmtime(&now);
	if (gmt == NULL)
		return std::string();
	if (std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt) == 0)
		return std::string();
	return std::string(buffer);
}

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
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 431: return "Request Header Fields Too Large";
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
	bool hasDate = false;
	bool hasServer = false;

	for (it = headers.begin(); it != headers.end(); ++it)
	{
		std::string name = it->first;
		for (std::size_t i = 0; i < name.size(); ++i)
			name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));

		if (name == "content-length")
			continue;
		if (name == "date")
			hasDate = true;
		if (name == "server")
			hasServer = true;

		output << it->first << ": " << it->second << "\r\n";
	}

	if (!hasDate)
	{
		const std::string date = httpDate();
		if (!date.empty())
			output << "Date: " << date << "\r\n";
	}
	if (!hasServer)
		output << "Server: webserv/1.0\r\n";

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

HttpResponse ResponseBuilder::makeError(int statusCode, const std::string &body,
										const ServerConfig& config)
{
	HttpResponse response = makeError(statusCode, body);

	if (statusCode >= 400)
	{
		const std::map<int, std::string> &pages = config.getErrorPages();
		std::map<int, std::string>::const_iterator page = pages.find(statusCode);
		if (page != pages.end())
		{
			std::ifstream file(page->second.c_str(), std::ios::in | std::ios::binary);
			if (file)
			{
				std::ostringstream customBody;
				customBody << file.rdbuf();
				response.setBody(customBody.str());
				response.setHeader("Content-Type", "text/html");
			}
		}
	}
	return response;
}
