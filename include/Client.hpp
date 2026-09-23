#pragma once

#include <string>
#include <ctime>

class Client
{
private:
    int         _fd;

    std::string _readBuffer;
    std::string _writeBuffer;

    std::time_t _lastActivity;

    bool        _responsePending;
    bool        _closed;

public:
    Client(int fd);
    ~Client();

    // Identification
    int getFd() const;

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