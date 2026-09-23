#pragma once

#include <string>

class ServerSocket
{
private:
    int         _fd;
    std::string _host;
    int         _port;

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