// #include <iostream>
// #include "Server.hpp"

// int main()
// {
// 	Server server;
// 	(void)server;
// 	std::cout << "Hello world" << std::endl;
// 	return 0;
// }

#include "include/Server.hpp"
#include "include/ServerConfig.hpp"
#include "include/Location.hpp"

int main()
{
	ServerConfig config;

	config.addListenPort(8080);

	Location location;
	location.setPath("/");
	location.setRoot("./www");
	location.setIndex("index.html");
	location.setAutoindex(false);
	location.allowedMethod("GET");
	config.addLocation(location);

	Server server(config);

	if (!server.setup())
	{
		std::cerr << "Failed to setup server" << std::endl;
		return 1;
	}

	server.run();
	return 0;
}