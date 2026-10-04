#include "../include/Config.hpp"

void Config::addServer(const ServerConfig &server)
{
	servers.push_back(server);
}

const std::vector<ServerConfig> &Config::getServers() const
{
	return servers;
}

bool Config::validate() const
{
	if (servers.empty())
		return false;

	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		const ServerConfig &server = servers[i];

		if (server.getListenPorts().empty())
			return false;

		const std::vector<int> &ports = server.getListenPorts();

		for (std::size_t j = 0; j < ports.size(); ++j)
		{
			if (ports[j] < 1 || ports[j] > 65535)
				return false;
		}
    }

	return true;
}