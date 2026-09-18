#pragma once

#include <vector>
#include <map>
#include <string>
#include "Location.hpp"

class ServerConfig
{
private:
	std::vector<int> listen_ports;
	std::vector<Location> locations;
	std::map<int, std::string> error_pages;
	std::size_t max_body_size;

public:
	ServerConfig();
	ServerConfig(const ServerConfig &other);
	ServerConfig &operator=(const ServerConfig &other);
	~ServerConfig();

	void addListenPort(int port);
	void addLocation(const Location &location);
	void addErrorPage(int statusCode, const std::string &path);
	void setMaxBodySize(std::size_t size);
	const std::vector<int> &getListenPorts() const;
	const std::vector<Location> &getLocations() const;
	const std::map<int, std::string> &getErrorPages() const;
	std::size_t getMaxBodySize() const;
	const Location* findLocation(const std::string &path) const;
};