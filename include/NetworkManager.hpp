#pragma once

#include <vector>
#include <map>
#include <poll.h>

#include "ServerSocket.hpp"
#include "Client.hpp"

class NetworkManager
{
private:
    std::vector<ServerSocket> _servers;

    std::map<int, Client>     _clients;

    std::vector<pollfd>       _pollFds;

    int                       _connectionTimeout;

public:
    NetworkManager();
    ~NetworkManager();

    // Server/listener management
    void addServer(const std::string& host, int port);

    void initializeServers();

    // Main networking loop
    void run();

    // poll()
    void buildPollFds();
    void processEvents();

    // Listener events
    void handleNewConnection(ServerSocket& server);

    // Client events
    void handleClientRead(int clientFd);
    void handleClientWrite(int clientFd);

    // Client management
    void addClient(int clientFd);
    void removeClient(int clientFd);

    // Timeout handling
    void checkTimeouts();

    // Interface toward HTTP/application layer
    std::string receiveRequest(int clientFd);
    void sendResponse(int clientFd, const std::string& response);

    // Cleanup
    void shutdown();
};