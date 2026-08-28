#include "Server.hpp"

Server::Server(int port) : _server_fd(-1), _port(port)
{
}

Server::~Server()
{
	if (_server_fd != -1)
		close(_server_fd);
}

void Server::create_socket()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd == -1)
    {
        perror("socket");
        return;
    }
    std::cout << "Socket created: fd=" << _server_fd << std::endl;
}

void Server::set_sock_opts()
{
    int opt = 1;

    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("setsockopt");
}

struct sockaddr_in Server::create_addr() const
{
    struct sockaddr_in address;

    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);
    return address;
}

bool Server::bind_socket(const struct sockaddr_in& address)
{
    if (bind(_server_fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) == -1)
    {
        perror("bind");
        return false;
    }
    std::cout << "Bound to port " << _port << std::endl;
    return true;
}

bool Server::listen_socket()
{
    if (listen(_server_fd, SOMAXCONN) == -1)
    {
        perror("listen");
        return false;
    }
    std::cout << "Listening on port " << _port << std::endl;
    return true;
}

int Server::accept_client()
{
    std::cout << "Waiting for client..." << std::endl;

    int clientFd = accept(_server_fd, NULL, NULL);
    if (clientFd == -1)
    {
        perror("accept");
        return -1;
    }

    std::cout << "Client connected: fd=" << clientFd << std::endl;
    return (clientFd);
}

void Server::start()
{
    create_socket();
    if (_server_fd == -1)
        return;

    set_sock_opts();

    struct sockaddr_in address = create_addr();
    if (!bind_socket(address))
        return;

    if (!listen_socket())
        return;

    accept_client();
}
