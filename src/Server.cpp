#include "../include/Server.hpp"

Server::Server(const ServerConfig &config) : config(config)
{}

Server::~Server()
{
	for (size_t i = 0; i < listen_fds.size(); i++)
		close(listen_fds[i]);
}

bool Server::createSocket(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1)
	{
		std::cerr << "socket() failed" << std::endl;
		return false;
	}

	int opt = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		std::cerr << "setsockopt() failed" << std::endl;
		return (close(fd), false);
	}

	struct sockaddr_in address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) == -1)
	{
		std::cerr << "bind() failed" << std::endl;
		return (close(fd), false);
	}
	if (listen(fd, SOMAXCONN) == -1)
	{
		std::cerr << "listen() failed" << std::endl;
		return (close(fd), false);
	}

	listen_fds.push_back(fd);
	std::cout << "Server listening on port " << port << std::endl;
	return true;
}

bool Server::setup()
{
	const std::vector<int>& ports = config.getListenPorts();

	for (std::size_t i = 0; i < ports.size(); ++i)
	{
		if (!createSocket(ports[i]))
			return false;
	}

	return true;
}

void Server::run()
{
	std::cout << "Server is running" << std::endl;

	while(true)
	{}
}