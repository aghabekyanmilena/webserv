#pragma once

#include <string>
#include <vector>
#include "ServerConfig.hpp"

class Config
{
private:
	std::vector<ServerConfig> servers;

public:
	// Throws std::runtime_error on invalid input; preserves existing config on failure.
	void parseFile(const std::string &filename);
	void addServer(const ServerConfig &server);
	const std::vector<ServerConfig> &getServers() const;
	bool validate() const;
};
