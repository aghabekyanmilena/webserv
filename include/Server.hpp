#pragma once

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

class Server
{
private:
    int _server_fd;
    int _port;

    void create_socket();
    void set_sock_opts();
    struct sockaddr_in create_addr() const;
    bool bind_socket(const struct sockaddr_in& address);
    bool listen_socket();
    int accept_client();

public:
    Server(int port);
    ~Server();

    void start();
};
