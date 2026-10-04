#include "NetworkManager.hpp"
#include "Config.hpp"
#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "ResponseBuilder.hpp"
#include <iostream>
#include <exception>
#include <fstream>
#include <sstream>

static bool processRequest(const std::string& raw, const ServerConfig& config,
                           std::string& serialized)
{
    HttpRequest parsed;
    HttpParser parser;
    const HttpParser::ParseResult result = parser.parse(raw, parsed);
    HTTPResponse response;
    const bool tooLarge = parsed.getContentLength() > config.getMaxBodySize()
        || parsed.getBody().size() > config.getMaxBodySize();
    if (!tooLarge && result == HttpParser::INCOMPLETE)
        return false;

    if (tooLarge)
        response.statusCode = 413;
    else if (result == HttpParser::ERROR)
        response.statusCode = 400;
    else
    {
        HTTPRequest request;
        request.method = parsed.getMethod();
        request.uri = parsed.getPath();
        request.body = parsed.getBody();
        request.headers = parsed.getHeaders();
        RequestHandler handler;
        response = handler.handleRequest(request, config);
    }
    if (response.statusCode >= 400)
    {
        const std::map<int, std::string>& pages = config.getErrorPages();
        std::map<int, std::string>::const_iterator page = pages.find(response.statusCode);
        if (page != pages.end())
        {
            std::ifstream file(page->second.c_str(), std::ios::in | std::ios::binary);
            if (file)
            {
                std::ostringstream body;
                body << file.rdbuf();
                response.body = body.str();
                response.headers["Content-Type"] = "text/html";
            }
        }
    }
    HttpResponse formatted = ResponseBuilder::makeError(response.statusCode, response.body);
    for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
         it != response.headers.end(); ++it)
        formatted.setHeader(it->first, it->second);
    formatted.setHeader("Connection", "close");
    serialized = ResponseBuilder::serialize(formatted);
    return true;
}

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [config.conf]" << std::endl;
        return 1;
    }
    try
    {
        Config config;
        config.parseFile(argc == 2 ? argv[1] : "configs/webserv.conf");
        NetworkManager network;
        network.setRequestProcessor(processRequest);
        const std::vector<ServerConfig>& servers = config.getServers();
        for (std::size_t i = 0; i < servers.size(); ++i)
            network.addServer(servers[i]);
        network.initializeServers();
        network.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
