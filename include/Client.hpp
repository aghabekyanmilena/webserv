#pragma once

#include <string>
#include <ctime>
#include <iostream>
#include "ServerConfig.hpp"

class Client
{
private:
    int         _fd;
    ServerConfig _config;

    std::string _readBuffer;
    std::string _writeBuffer;

    std::time_t _lastActivity;

    bool        _responsePending;
    bool        _closed;

public:
    Client(int fd);
    Client(int fd, const ServerConfig& config);
    ~Client();

    // Identification
    int getFd() const;
    const ServerConfig& getConfig() const;

    // Receiving
    bool receiveData();

    const std::string& getReadBuffer() const;
    void clearReadBuffer();

    // Sending
    void setResponse(const std::string& response);
    bool sendData();

    bool hasPendingResponse() const;

    // Timeout
    void updateActivity();
    std::time_t getLastActivity() const;
    bool hasTimedOut(std::time_t now, int timeoutSeconds) const;

    // State
    bool isClosed() const;
    void markClosed();

    // Cleanup
    void closeConnection();
};