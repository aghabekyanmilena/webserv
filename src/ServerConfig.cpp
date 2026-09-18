#include "../include/ServerConfig.hpp"

ServerConfig::ServerConfig() : max_body_size(0) {}

ServerConfig::ServerConfig(const ServerConfig &other)
	: listen_ports(other.listen_ports),
	locations(other.locations),
	error_pages(other.error_pages),
	max_body_size(other.max_body_size)
{}

ServerConfig &ServerConfig::operator=(const ServerConfig &other)
{
	if (this != &other)
	{
		listen_ports = other.listen_ports;
		locations = other.locations;
		error_pages = other.error_pages;
		max_body_size = other.max_body_size;
	}
	return *this;
}

ServerConfig::~ServerConfig() {}

void ServerConfig::addListenPort(int port)
{
	listen_ports.push_back(port);
}

void ServerConfig::addLocation(const Location &location)
{
	locations.push_back(location);
}

void ServerConfig::addErrorPage(int statusCode, const std::string &path)
{
	error_pages[statusCode] = path;
}

void ServerConfig::setMaxBodySize(size_t size)
{
	max_body_size = size;
}

const std::vector<int> &ServerConfig::getListenPorts() const
{
	return listen_ports;
}

const std::vector<Location>& ServerConfig::getLocations() const
{
	return locations;
}

const std::map<int, std::string> &ServerConfig::getErrorPages() const
{
	return error_pages;
}

size_t ServerConfig::getMaxBodySize() const
{
	return max_body_size;
}