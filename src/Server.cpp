#include "Server.hpp"

Server::Server(int port) : _server_fd(-1), _port(port)
{
    std::cout << "Server listening on " << port << " port." << std::endl;
}

Server::~Server()
{
	if (_server_fd != -1)
		close(_server_fd);
}

void Server::start()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd == -1)
    {
        perror("socket");
        return;
    }
    std::cout << "Socket created: fd=" << _server_fd << std::endl;
}
