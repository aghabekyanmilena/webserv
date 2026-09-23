#include "ServerSocket.hpp"

ServerSocket::ServerSocket(const std::string& host, int port) : _host(host), _port(port), _fd(-1) { }

ServerSocket::~ServerSocket()
{
	closeSocket();
}


bool ServerSocket::create()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
    {
        perror("socket");
        return false;
    }

    std::cout << "Socket created: fd=" << _fd << std::endl;
	return true;
}

struct sockaddr_in ServerSocket::create_addr() const
{
    struct sockaddr_in address;

    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);
    return address;
}

bool ServerSocket::bindSocket()
{
	struct sockaddr_in address = create_addr();
	
    if(bind(_fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) == -1)
    {
        perror("bind");
        return false;
    }

    std::cout << "Bound to port " << _port << std::endl;
    return true;
}


int ServerSocket::getFd() const
{
	return _fd;
}

int ServerSocket::getPort() const
{
	return _port;
}

const std::string& ServerSocket::getHost() const
{
	return _host;
}


void ServerSocket::closeSocket()
{
	if (_fd != -1)
		close(_fd);
}