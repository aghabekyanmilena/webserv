#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

class ServerSocket
{
private:
    int         _fd;
    std::string _host;
    int         _port;

    struct sockaddr_in create_addr() const;
public:
    ServerSocket(const std::string& host, int port);
    ~ServerSocket();

    // Setup
    bool create();
    bool bindSocket();
    bool listenSocket();
    bool setNonBlocking();

    // Connection
    int acceptClient();

    // Getters
    int getFd() const;
    int getPort() const;
    const std::string& getHost() const;

    // Cleanup
    void closeSocket();
};