#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "Location.hpp"

class ServerConfig
{
private:
	std::vector<int> listenPorts;
	std::vector<Location> locations;
	std::map<int, std::string> errorPages;

	size_t maxBodySize;

public:
	void addListenPort(int port);
	void addLocation(const Location& location);

	// getters

	const Location* findLocation(const std::string& path) const;
}