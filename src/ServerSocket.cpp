#include "ServerSocket.hpp"

#include <sstream>
#include <cerrno>
#include <cstring>

static bool parseIPv4Address(const std::string& host, unsigned long& address)
{
    if (host.empty() || host == "*" || host == "0.0.0.0")
    {
        address = INADDR_ANY;
        return true;
    }

    std::istringstream stream(host);
    unsigned int octet1;
    unsigned int octet2;
    unsigned int octet3;
    unsigned int octet4;
    char dot1;
    char dot2;
    char dot3;

    if (!(stream >> octet1 >> dot1 >> octet2 >> dot2 >> octet3 >> dot3 >> octet4))
        return false;

    if (dot1 != '.' || dot2 != '.' || dot3 != '.')
        return false;

    if (octet1 > 255 || octet2 > 255 || octet3 > 255 || octet4 > 255)
        return false;

    stream >> std::ws;
    if (!stream.eof())
        return false;

    address = (static_cast<unsigned long>(octet1) << 24) | (static_cast<unsigned long>(octet2) << 16) | (static_cast<unsigned long>(octet3) << 8) | static_cast<unsigned long>(octet4);
    return true;
}

ServerSocket::ServerSocket(const std::string& host, int port) : _fd(-1), _host(host), _port(port), _config()
{
    _config.setHost(host);
    _config.addListenPort(port);
}

ServerSocket::ServerSocket(const ServerConfig& config, int port) : _fd(-1), _host(config.getHost()), _port(port), _config(config) { }

ServerSocket::~ServerSocket()
{
    closeSocket();
}

bool ServerSocket::create()
{
    if (_fd != -1)
        closeSocket();

    _fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (_fd == -1)
    {
        std::cerr << "socket() failed for " << _host << ":" << _port << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    int reuseAddress = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) == -1)
    {
        std::cerr << "setsockopt(SO_REUSEADDR) failed for " << _host << ":" << _port << ": " << std::strerror(errno) << std::endl;
        closeSocket();
        return false;
    }

    return true;
}

struct sockaddr_in ServerSocket::create_addr() const
{
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));

    unsigned long hostAddress = INADDR_ANY;
    parseIPv4Address(_host, hostAddress);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(hostAddress);
    address.sin_port = htons(static_cast<unsigned short>(_port));
    return address;
}

bool ServerSocket::bindSocket()
{
    if (_fd == -1 || _port < 0 || _port > 65535)
        return false;

    unsigned long ignoredAddress;
    if (!parseIPv4Address(_host, ignoredAddress))
    {
        std::cerr << "Invalid IPv4 listen address: " << _host << std::endl;
        return false;
    }

    const struct sockaddr_in address = create_addr();

    if (bind(_fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) == -1)
    {
        std::cerr << "bind() failed for " << _host << ":" << _port << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool ServerSocket::listenSocket()
{
    if (_fd == -1)
        return false;

    if (listen(_fd, SOMAXCONN) == -1)
    {
        std::cerr << "listen() failed for " << _host << ":" << _port << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

int ServerSocket::acceptClient(struct sockaddr_in* peer)
{
    if (_fd == -1)
        return -1;

    socklen_t length = sizeof(struct sockaddr_in);
    const int clientFd = accept(_fd, reinterpret_cast<struct sockaddr*>(peer), (peer == NULL) ? NULL : &length);

    if (clientFd == -1)
        return -1;

    return clientFd;
}

const ServerConfig& ServerSocket::getConfig() const
{
    return _config;
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
    {
        close(_fd);
        _fd = -1;
    }
}
