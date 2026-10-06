#pragma once

#include <string>
#include <ctime>
#include "ServerConfig.hpp"

class Client
{
private:
	int          _fd;
	int          _listenPort;
	ServerConfig _config;
	std::string  _readBuffer;
	std::string  _writeBuffer;
	std::time_t  _lastActivity;
	bool         _responsePending;
	bool         _closed;

public:
	Client(int fd);
	Client(int fd, const ServerConfig& config);
	Client(int fd, const ServerConfig& config, int listenPort);
	~Client();

	int getFd() const;
	int getListenPort() const;
	const ServerConfig& getConfig() const;
	void setConfig(const ServerConfig& config);

	bool receiveData();
	const std::string& getReadBuffer() const;
	void clearReadBuffer();

	void setResponse(const std::string& response);
	bool sendData();
	bool hasPendingResponse() const;

	void updateActivity();
	std::time_t getLastActivity() const;
	bool hasTimedOut(std::time_t now, int timeoutSeconds) const;

	bool isClosed() const;
	void markClosed();
	void closeConnection();
};
