#include "include/NetworkManager.hpp"
#include "include/Config.hpp"
#include "include/HttpParser.hpp"
#include "include/RequestHandler.hpp"
#include "include/ResponseBuilder.hpp"
#include <iostream>
#include <exception>

static std::string serializeResponse(const HTTPResponse& response, const ServerConfig& config,
    bool preserveBody = false)
{
	HttpResponse formatted;
	if (preserveBody)
	{
		formatted.setStatusCode(response.statusCode);
		formatted.setReasonPhrase(ResponseBuilder::reasonPhrase(response.statusCode));
		formatted.setBody(response.body);
	}
	else
		formatted = ResponseBuilder::makeError(response.statusCode, response.body, config);
	for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
		 it != response.headers.end(); ++it)
		formatted.setHeader(it->first, it->second);
	formatted.setHeader("Connection", "close");
	return ResponseBuilder::serialize(formatted);
}

static std::string applicationPath(const std::string& decoded)
{
	// HttpParser already decoded the URI. Application APIs expect an encoded
	// path and decode it once more; escape literal '%' to prevent double decoding.
	std::string encoded;
	for (std::size_t i = 0; i < decoded.size(); ++i)
		if (decoded[i] == '%') encoded += "%25";
		else encoded += decoded[i];
	return encoded;
}

static std::string processCgiResponse(const std::string& output, const ServerConfig& config)
{
	RequestHandler handler;
	const HTTPResponse response = handler.parseCgiOutput(output);
	// Valid CGI output already supplies the entity body, including empty error
	// bodies. Preserve it; malformed output follows the normal error-page path.
	return serializeResponse(response, config, response.headers.count("Content-Length") != 0);
}

static bool processRequest(const std::string &raw, const ServerConfig &config, std::string &serialized,
    CgiRequest& plan, const CgiContext& clientContext)
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
		request.uri = applicationPath(parsed.getPath());
		request.query = parsed.getQuery();
		request.body = parsed.getBody();
		request.headers = parsed.getHeaders();
		RequestHandler handler;
		CgiContext context = clientContext;
		context.query = parsed.getQuery();
		context.protocol = parsed.getVersion();
		const RequestHandler::CgiResult cgi = handler.prepareCgi(request, config, context, plan, response);
		if (cgi == RequestHandler::CGI_READY)
			return true;
		// Failed preparation may have partially filled the plan. Never execute it.
		plan = CgiRequest();
		if (cgi == RequestHandler::CGI_NOT_SELECTED)
			response = handler.handleRequest(request, config);
	}

	serialized = serializeResponse(response, config);
	return true;
}

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
		return 1;
	}
	try
	{
		Config config;
		config.parseFile(argc == 2 ? argv[1] : "configs/webserv.conf");
		NetworkManager network;
		network.setAsyncRequestProcessor(processRequest, processCgiResponse);
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
	catch (...)
	{
		std::cerr << "Unexpected startup failure" << std::endl;
		return 1;
	}
	return 0;
}
