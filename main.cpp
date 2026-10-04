#include "NetworkManager.hpp"
#include "include/Config.hpp"
#include <iostream>
#include <exception>

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " [config.conf]" << std::endl;
		return 1;
	}
	try
	{
		Config config;
		config.parseFile(argv[1]);
		NetworkManager network;
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