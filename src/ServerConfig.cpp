#include "../include/ServerConfig.hpp"

ServerConfig::ServerConfig() : max_body_size(1000000), host("127.0.0.1") {}

ServerConfig::ServerConfig(const ServerConfig &other)
	: listen_ports(other.listen_ports),
	locations(other.locations),
	error_pages(other.error_pages),
	max_body_size(other.max_body_size), host(other.host),
	server_name(other.server_name), root(other.root)
{}

ServerConfig &ServerConfig::operator=(const ServerConfig &other)
{
	if (this != &other)
	{
		listen_ports = other.listen_ports;
		locations = other.locations;
		error_pages = other.error_pages;
		max_body_size = other.max_body_size;
		host = other.host;
		server_name = other.server_name;
		root = other.root;
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

const Location *ServerConfig::findLocation(const std::string &path) const
{
	for (std::size_t i = 0; i < locations.size(); ++i)
	{
		if (path.find(locations[i].getPath()) == 0)
			return &locations[i];
	}
	return NULL;
}
void ServerConfig::setHost(const std::string &value) { host = value; }
void ServerConfig::setServerName(const std::string &value) { server_name = value; }
void ServerConfig::setRoot(const std::string &value) { root = value; }
const std::string &ServerConfig::getHost() const { return host; }
const std::string &ServerConfig::getServerName() const { return server_name; }
const std::string &ServerConfig::getRoot() const { return root; }
