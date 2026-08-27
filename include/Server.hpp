#pragma once

#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

class Server
{
private:
    int _server_fd;
    int _port;

public:
    Server(int port);
    ~Server();

    void start();
};