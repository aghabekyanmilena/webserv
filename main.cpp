#include "include/Config.hpp"
#include <iostream>
#include <exception>

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [config.conf]" << std::endl;
        return 1;
    }
    try
    {
        Config config;
        config.parseFile(argc == 2 ? argv[1] : "webserv.conf");
        const std::vector<ServerConfig> &servers = config.getServers();
        for (std::size_t i = 0; i < servers.size(); ++i)
        {
            const ServerConfig &server = servers[i];
            std::cout << "Server " << i + 1 << ": host=" << server.getHost()
                      << " name=" << server.getServerName()
                      << " root=" << server.getRoot() << " ports=";
            for (std::size_t j = 0; j < server.getListenPorts().size(); ++j)
                std::cout << (j ? "," : "") << server.getListenPorts()[j];
            std::cout << " locations=" << server.getLocations().size()
                      << " max_body_size=" << server.getMaxBodySize() << std::endl;
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
