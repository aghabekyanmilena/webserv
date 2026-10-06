#include "NetworkManager.hpp"
#include "ResponseBuilder.hpp"

#include <ctime>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <sstream>

namespace
{
    volatile sig_atomic_t stopRequested = 0;
    void requestStop(int) { stopRequested = 1; }

    std::string errorResponse(int status)
    {
        HttpResponse response = ResponseBuilder::makeError(status, "");
        response.setHeader("Connection", "close");
        return ResponseBuilder::serialize(response);
    }
}

NetworkManager::NetworkManager()
    : _servers(), _clients(), _pollFds(), _connectionTimeout(30),
      _listenersPausedUntil(0), _requestProcessor(NULL)
{
    /* A send() to a client that disappeared must not terminate the server. */
    signal(SIGPIPE, SIG_IGN);
}

void NetworkManager::setRequestProcessor(RequestProcessor processor)
{
    _requestProcessor = processor;
}

NetworkManager::~NetworkManager()
{
    shutdown();
}

void NetworkManager::addServer(const std::string& host, int port)
{
    ServerConfig config;
    config.setHost(host);
    config.addListenPort(port);
    addServer(config);
}

void NetworkManager::addServer(const ServerConfig& config)
{
    // Register all configurations before opening sockets: vector growth must
    // never copy and destroy a ServerSocket owning an active descriptor.
    for (std::size_t i = 0; i < _servers.size(); ++i)
        if (_servers[i].getFd() != -1)
            throw std::runtime_error("Add server configurations before initialization");

    const std::vector<int>& ports = config.getListenPorts();
    if (ports.empty())
        throw std::runtime_error("Server configuration requires a listen port");
    for (std::size_t i = 0; i < ports.size(); ++i)
    {
        if (ports[i] < 1 || ports[i] > 65535)
            throw std::runtime_error("Invalid configured listen port");
        _servers.push_back(ServerSocket(config, ports[i]));
    }
}

void NetworkManager::initializeServers()
{
    for (std::vector<ServerSocket>::iterator it = _servers.begin();
         it != _servers.end(); ++it)
    {
        if (!it->create() || !it->bindSocket() || !it->listenSocket())
        {
            it->closeSocket();
            std::ostringstream message;
            message << "Cannot listen on " << it->getHost() << ":" << it->getPort();
            throw std::runtime_error(message.str());
        }

        std::cout << "Listening on " << it->getHost()
                  << ":" << it->getPort()
                  << " (fd=" << it->getFd() << ")" << std::endl;
    }
}

void NetworkManager::run()
{
    stopRequested = 0;
    signal(SIGINT, requestStop);
    signal(SIGTERM, requestStop);
    while (!stopRequested)
    {
        buildPollFds();

        if (_pollFds.empty())
        {
            bool hasListener = false;
            for (std::size_t i = 0; i < _servers.size(); ++i)
                hasListener = hasListener || _servers[i].getFd() != -1;
            if (!hasListener && _clients.empty())
                throw std::runtime_error("No active listening sockets or clients");
            poll(NULL, 0, 1000);
            checkTimeouts();
            continue;
        }

        /*
         * This is the single poll() used for all socket I/O.  A finite poll
         * timeout lets us enforce connection timeouts without a second I/O
         * readiness mechanism.
         */
        const int readyCount = poll(&_pollFds[0], _pollFds.size(), 1000);

        if (readyCount > 0)
            processEvents();
        else if (readyCount < 0)
        {
            /* Do not inspect errno.  A transient poll interruption simply
             * causes the event loop to rebuild the set and try again. */
        }

        checkTimeouts();
    }
}

void NetworkManager::buildPollFds()
{
    std::vector<pollfd> nextPollFds;

    try
    {
        for (std::vector<ServerSocket>::const_iterator it = _servers.begin();
             it != _servers.end(); ++it)
        {
            if (it->getFd() == -1 || std::time(NULL) < _listenersPausedUntil)
                continue;

            struct pollfd descriptor;
            descriptor.fd = it->getFd();
            descriptor.events = POLLIN;
            descriptor.revents = 0;
            nextPollFds.push_back(descriptor);
        }

        for (std::map<int, Client>::const_iterator it = _clients.begin();
             it != _clients.end(); ++it)
        {
            if (it->second.isClosed())
                continue;

            struct pollfd descriptor;
            descriptor.fd = it->first;
            descriptor.events = it->second.hasPendingResponse() ? POLLOUT : POLLIN;
            descriptor.revents = 0;
            nextPollFds.push_back(descriptor);
        }
    }
    catch (...)
    {
        /* Keep the previous valid poll set if rebuilding it runs out of
         * resources.  The next loop iteration can try rebuilding again. */
        return;
    }

    _pollFds.swap(nextPollFds);
}

void NetworkManager::processEvents()
{
    for (std::vector<pollfd>::const_iterator pollIt = _pollFds.begin();
         pollIt != _pollFds.end(); ++pollIt)
    {
        if (pollIt->revents == 0)
            continue;

        bool listenerFound = false;

        for (std::vector<ServerSocket>::iterator serverIt = _servers.begin();
             serverIt != _servers.end(); ++serverIt)
        {
            if (serverIt->getFd() != pollIt->fd)
                continue;

            listenerFound = true;

            if (pollIt->revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                serverIt->closeSocket();
                break;
            }

            if (pollIt->revents & POLLIN)
                handleNewConnection(*serverIt);

            break;
        }

        if (listenerFound)
            continue;

        std::map<int, Client>::iterator clientIt = _clients.find(pollIt->fd);
        if (clientIt == _clients.end())
            continue;

        if (pollIt->revents & (POLLERR | POLLNVAL))
        {
            removeClient(pollIt->fd);
            continue;
        }

        if (pollIt->revents & POLLIN)
            handleClientRead(pollIt->fd);

        clientIt = _clients.find(pollIt->fd);
        if (clientIt == _clients.end())
            continue;

        if (pollIt->revents & POLLHUP)
        {
            removeClient(pollIt->fd);
            continue;
        }

        if (pollIt->revents & POLLOUT)
            handleClientWrite(pollIt->fd);
    }
}

void NetworkManager::handleNewConnection(ServerSocket& server)
{
    /* Called only after poll() reported POLLIN for the listening socket. */
    const int clientFd = server.acceptClient();
    if (clientFd == -1)
    {
        _listenersPausedUntil = std::time(NULL) + 2;
        shedIdleClients();
        return;
    }

    addClient(clientFd, server.getConfig());
}

void NetworkManager::handleClientRead(int clientFd)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;

    /* Called only from a POLLIN event. */
    if (!it->second.receiveData())
    {
        removeClient(clientFd);
        return;
    }
    if (it->second.hasPendingResponse())
        return;
    if (it->second.getRequestError())
    {
        sendResponse(clientFd, errorResponse(it->second.getRequestError()));
        return;
    }
    if (_requestProcessor == NULL || !it->second.isRequestReady())
        return;
    try
    {
        std::string response;
        if (_requestProcessor(it->second.getReadBuffer(), it->second.getConfig(), response))
        {
            it->second.clearReadBuffer();
            sendResponse(clientFd, response);
        }
    }
    catch (...)
    {
        removeClient(clientFd);
    }
}

void NetworkManager::handleClientWrite(int clientFd)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;

    /* Called only from a POLLOUT event. */
    if (!it->second.sendData()
        || (_requestProcessor != NULL && !it->second.hasPendingResponse()))
        removeClient(clientFd);
}

void NetworkManager::addClient(int clientFd)
{
    addClient(clientFd, ServerConfig());
}

void NetworkManager::addClient(int clientFd, const ServerConfig& config)
{
    if (clientFd < 0 || _clients.find(clientFd) != _clients.end())
    {
        if (clientFd >= 0 && _clients.find(clientFd) == _clients.end())
            close(clientFd);
        return;
    }

    try
    {
        _clients.insert(std::make_pair(clientFd, Client(clientFd, config)));
    }
    catch (...)
    {
        close(clientFd);
    }
}

void NetworkManager::removeClient(int clientFd)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;

    it->second.closeConnection();
    _clients.erase(it);
}

void NetworkManager::checkTimeouts()
{
    const std::time_t now = std::time(NULL);
    std::vector<int> timedOutClients;
    std::vector<int> incompleteClients;

    try
    {
        for (std::map<int, Client>::const_iterator it = _clients.begin();
             it != _clients.end(); ++it)
        {
            if (it->second.hasRequestTimedOut(now, _connectionTimeout))
                incompleteClients.push_back(it->first);
            else if (it->second.hasTimedOut(now, _connectionTimeout))
                timedOutClients.push_back(it->first);
        }
    }
    catch (...)
    {
        return;
    }

    for (std::vector<int>::const_iterator it = incompleteClients.begin();
         it != incompleteClients.end(); ++it)
        sendResponse(*it, errorResponse(408));

    for (std::vector<int>::const_iterator it = timedOutClients.begin();
         it != timedOutClients.end(); ++it)
    {
        removeClient(*it);
    }
}

void NetworkManager::shedIdleClients()
{
    // Reclaim a descriptor from the least recently active unfinished request.
    std::map<int, Client>::const_iterator oldest = _clients.end();
    for (std::map<int, Client>::const_iterator it = _clients.begin();
         it != _clients.end(); ++it)
        if (it->second.isIdle() && (oldest == _clients.end()
            || it->second.getLastActivity() < oldest->second.getLastActivity()))
            oldest = it;
    if (oldest != _clients.end())
        removeClient(oldest->first);
}

const ServerConfig* NetworkManager::getClientConfig(int clientFd) const
{
    std::map<int, Client>::const_iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return NULL;
    return &it->second.getConfig();
}

std::string NetworkManager::receiveRequest(int clientFd)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return std::string();

    try
    {
        /* Networking does not decide whether this is a complete HTTP request.
         * It only hands the raw bytes received so far to the upper layer. */
        const std::string requestBytes = it->second.getReadBuffer();
        it->second.clearReadBuffer();
        return requestBytes;
    }
    catch (...)
    {
        removeClient(clientFd);
        return std::string();
    }
}

void NetworkManager::sendResponse(int clientFd, const std::string& response)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;

    /* Queue only.  The actual send() is deferred until poll() reports
     * POLLOUT, so application code never writes directly to the socket. */
    it->second.setResponse(response);
    it->second.markRequestComplete();
    if (it->second.isClosed())
        removeClient(clientFd);
}

void NetworkManager::shutdown()
{
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        it->second.closeConnection();
    }
    _clients.clear();

    for (std::vector<ServerSocket>::iterator it = _servers.begin();
         it != _servers.end(); ++it)
    {
        it->closeSocket();
    }

    _pollFds.clear();
}
