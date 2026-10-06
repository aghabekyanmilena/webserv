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
    std::time_t _requestStartedAt;
    std::size_t _headerEnd;
    std::size_t _bodyExpected;
    std::size_t _chunkPos;
    std::size_t _chunkSize;
    std::size_t _decodedBodySize;
    int         _chunkState;
    int         _requestError;
    bool        _chunked;
    bool        _requestComplete;
    bool        _requestReady;

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
    bool isRequestReady() const;
    int getRequestError() const;
    void markRequestComplete();

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
    bool hasRequestTimedOut(std::time_t now, int timeoutSeconds) const;
    bool isIdle() const;

    // State
    bool isClosed() const;
    void markClosed();

    // Cleanup
    void closeConnection();
};
