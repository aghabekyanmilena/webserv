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
	size_t max_body_size;
	std::string host;
	std::string server_name;
	std::string root;
	std::string index;

public:
	ServerConfig();
	ServerConfig(const ServerConfig &other);
	ServerConfig &operator=(const ServerConfig &other);
	~ServerConfig();

	void setHost(const std::string &value);
	void setServerName(const std::string &value);
	void setRoot(const std::string &value);
	void setIndex(const std::string &value);
	const std::string &getHost() const;
	const std::string &getServerName() const;
	const std::string &getRoot() const;
	const std::string &getIndex() const;

	void addListenPort(int port);
	void addLocation(const Location &location);
	void addErrorPage(int statusCode, const std::string &path);
	void setMaxBodySize(size_t size);

	const std::vector<int> &getListeningPorts() const;
	const std::vector<Location> &getLocations() const;
	const std::map<int, std::string> &getErrorPages() const;
	size_t getMaxBodySize() const;
	const Location *findLocation(const std::string &path) const;
};
