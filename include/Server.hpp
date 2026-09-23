#pragma once

#include <iostream>
#include <vector>
#include "ServerConfig.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>

class Server
{
private:
	std::vector<int> listen_fds;
	ServerConfig config;

	bool createSocket(int port);
public:
	Server(const ServerConfig &config);
	~Server();

	bool setup();
	void run();
};