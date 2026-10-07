#pragma once

#include <string>
#include <ctime>
#include "ServerConfig.hpp"

class Client
{
public:
    Client(int fd);
    Client(int fd, const ServerConfig& config);
    Client(int fd, const ServerConfig& config, int listenPort);
    ~Client();

    int getFd() const;
    int getListeningPort() const;
    const ServerConfig& getConfig() const;
    void setConfig(const ServerConfig& config);
    void setRemoteAddress(const std::string& address);
    const std::string& getRemoteAddress() const;

    bool receiveData();
    bool isRequestReady() const;
    int getRequestError() const;
    void setRequestComplete();
    const std::string& getReadBuffer() const;
    void clearReadBuffer();

    void setResponse(const std::string& response);
    void setEmergencyResponse(const std::string& response);
    bool sendData();
    bool hasPendingResponse() const;

    std::time_t getLastActivity() const;
    bool hasTimedOut(std::time_t now, int timeoutSeconds) const;
    bool hasRequestTimedOut(std::time_t now, int timeoutSeconds) const;
    bool hasDrainTimedOut(std::time_t now, int timeoutSeconds) const;
    bool isIdle() const;

    bool isClosed() const;
    void closeConnection();

private:
    int          _fd;
    int          _listeningPort;
    ServerConfig _config;
    std::string  _remoteAddr;
    std::string  _readBuf;
    std::string  _writeBuf;
    std::time_t  _lastActivity;
    std::time_t  _requestStartedAt;
    std::time_t  _responseQueuedAt;
    const std::string* _emergencyResponse;
    std::size_t  _emergencySent;
    std::size_t  _headerEnd;
    std::size_t  _bodyExpected;
    std::size_t  _chunkPos;
    std::size_t  _chunkSize;
    std::size_t  _decodedBodySize;
    int          _chunkState;
    int          _requestError;
    bool         _chunked;
    bool         _configSelected;
    bool         _requestComplete;
    bool         _requestReady;
    bool         _responsePending;
    bool         _closed;

    void update_last_activity();
    void set_closed();

};
