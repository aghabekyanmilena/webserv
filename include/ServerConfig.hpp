#pragma once

#include <iostream>
#include <vector>
#include <map>

class ServerConfig
{
private:
	std::vector<int> _listenPorts;
	std::vector<Location> _locations;
	std::map<int, std::string> _errorPages;

	size_t _maxBodySize;

public:
	void addListenPort(int port);
	void addLocation(const Location& location);

	// getters

	const Location* findLocation(const std::string& path) const;
}