#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ServerConfig.hpp"

class ServerSocket
{
public:
    ServerSocket(const std::string& host, int port);
    ServerSocket(const ServerConfig& config, int port);
    ~ServerSocket();

    bool create();
    bool bindSocket();
    bool listenSocket();

    int acceptClient(struct sockaddr_in* peer = NULL);

    const ServerConfig& getConfig() const;
    int getFd() const;
    int getPort() const;
    const std::string& getHost() const;

    void closeSocket();

private:
    int         _fd;
    std::string _host;
    int         _port;
    ServerConfig _config;

    struct sockaddr_in create_addr() const;

};
