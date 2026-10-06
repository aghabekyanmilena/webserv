#include "include/NetworkManager.hpp"
#include "include/Config.hpp"
#include "include/HttpParser.hpp"
#include "include/RequestHandler.hpp"
#include "include/ResponseBuilder.hpp"
#include <iostream>
#include <exception>

static bool processRequest(const std::string &raw, const ServerConfig &config, std::string &serialized)
{
	HttpRequest parsed;
	HttpParser parser;
	const std::size_t maxBody = config.getMaxBodySize();
	const HttpParser::ParseResult result = parser.parse(raw, parsed, maxBody);
	HTTPResponse response;

	if (result == HttpParser::INCOMPLETE)
		return false;

	if (result == HttpParser::HEADER_TOO_LARGE)
		response.statusCode = 431;
	else if (result == HttpParser::TOO_LARGE
		|| parsed.getContentLength() > maxBody
		|| parsed.getBody().size() > maxBody)
		response.statusCode = 413;
	else if (result == HttpParser::ERROR)
		response.statusCode = 400;
	else
	{
		HTTPRequest request;
		request.method = parsed.getMethod();
		request.uri = parsed.getPath();
		request.query = parsed.getQuery();
		request.body = parsed.getBody();
		request.headers = parsed.getHeaders();
		RequestHandler handler;
		response = handler.handleRequest(request, config);
	}

	HttpResponse formatted = ResponseBuilder::makeError(response.statusCode, response.body, config);
	for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
		 it != response.headers.end(); ++it)
		formatted.setHeader(it->first, it->second);
	formatted.setHeader("Connection", "close");
	serialized = ResponseBuilder::serialize(formatted);
	return true;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
		return 1;
	}
	try
	{
		Config config;
		config.parseFile(argv[1]);
		NetworkManager network;
		network.setRequestProcessor(processRequest);
		const std::vector<ServerConfig> &servers = config.getServers();
		for (std::size_t i = 0; i < servers.size(); ++i)
			network.addServer(servers[i]);
		network.initializeServers();
		network.run();
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << std::endl;
		return 1;
	}
	return 0;
}
