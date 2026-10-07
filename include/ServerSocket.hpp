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
private:
    int         _fd;
    std::string _host;
    int         _port;
    ServerConfig _config;

    struct sockaddr_in create_addr() const;
public:
    ServerSocket(const std::string& host, int port);
    ServerSocket(const ServerConfig& config, int port);
    ~ServerSocket();

    // Setup
    bool create();
    bool bindSocket();
    bool listenSocket();

    // Connection
    int acceptClient(struct sockaddr_in* peer = NULL);

    // Getters
    const ServerConfig& getConfig() const;
    int getFd() const;
    int getPort() const;
    const std::string& getHost() const;

    // Cleanup
    void closeSocket();
};
