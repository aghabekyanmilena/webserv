#include <iostream>
#include "Server.hpp"

//Arish's part
#include "Config.hpp"
#include "HttpTypes.hpp"
#include "RequestHandler.hpp"

// Arishi party check anelu hamara
static void printResponse(const HTTPResponse& response)
{
    std::cout << "Status: "
              << response.statusCode
              << std::endl;

    std::cout << "Body: "
              << response.body
              << std::endl;

    std::map<std::string, std::string>::const_iterator it;

    for (it = response.headers.begin();
         it != response.headers.end();
         ++it)
    {
        std::cout << it->first
                  << ": "
                  << it->second
                  << std::endl;
    }
}


int main()
{
	//Anulya part
	Server server;
	(void)server;
	std::cout << "Hello world" << std::endl;

	//Arish part
	ServerConfig config;
    LocationConfig rootLocation;
    rootLocation.path = "/";
    rootLocation.root = "./www";
    rootLocation.index = "index.html";
    rootLocation.autoindex = false;
    rootLocation.allowedMethods.push_back("GET");
    config.locations.push_back(rootLocation);
    LocationConfig uploadLocation;
    uploadLocation.path = "/uploads";
    uploadLocation.root = "./www/uploads";
    uploadLocation.index = "index.html";
    uploadLocation.autoindex = true;
    uploadLocation.allowedMethods.push_back("GET");
    uploadLocation.allowedMethods.push_back("POST");
    uploadLocation.allowedMethods.push_back("DELETE");
    config.locations.push_back(uploadLocation);
    HTTPRequest request;
    request.method = "DELETE";
    request.uri = "/images/cat.txt";
    RequestHandler handler;
    HTTPResponse response = handler.handleRequest(request, config);
    printResponse(response);

	return 0;
}
