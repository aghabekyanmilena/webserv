#include "NetworkManager.hpp"
#include "ResponseBuilder.hpp"

#include <ctime>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <unistd.h>
#include <cctype>
#include <fcntl.h>

namespace
{
	std::string errorResponse(int status, const ServerConfig& config)
	{
		HttpResponse response = ResponseBuilder::makeError(status, "", config);
		response.setHeader("Connection", "close");
		return ResponseBuilder::serialize(response);
	}
}

volatile sig_atomic_t NetworkManager::_stopRequested = 0;

static bool sameText(const std::string& left, const std::string& right)
{
	if (left.size() != right.size())
		return false;
	for (std::size_t i = 0; i < left.size(); ++i)
	{
		if (std::tolower(static_cast<unsigned char>(left[i]))
			!= std::tolower(static_cast<unsigned char>(right[i])))
			return false;
	}
	return true;
}

NetworkManager::NetworkManager()
    : _servers(), _serverConfigs(), _clients(), _pollFds(), _extraFds(),
      _connectionTimeout(30), _listenersPausedUntil(0), _requestProcessor(NULL)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &NetworkManager::handleSignal);
	signal(SIGTERM, &NetworkManager::handleSignal);
}

NetworkManager::~NetworkManager()
{
	shutdown();
}

void NetworkManager::handleSignal(int signalNumber)
{
	if (signalNumber == SIGINT || signalNumber == SIGTERM)
		_stopRequested = 1;
}

void NetworkManager::setRequestProcessor(RequestProcessor processor)
{
	_requestProcessor = processor;
}

void NetworkManager::addServer(const std::string& host, int port)
{
	ServerConfig config;
	config.setHost(host);
	config.addListenPort(port);
	addServer(config);
}

bool NetworkManager::hasListener(const std::string& host, int port) const
{
	for (std::size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].getHost() == host && _servers[i].getPort() == port)
			return true;
	}
	return false;
}

void NetworkManager::addServer(const ServerConfig& config)
{
	for (std::size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].getFd() != -1)
			throw std::runtime_error("Add server configurations before initialization");
	}

	const std::vector<int>& ports = config.getListenPorts();
	if (ports.empty())
		throw std::runtime_error("Server configuration requires a listen port");

	for (std::size_t i = 0; i < ports.size(); ++i)
	{
		if (ports[i] < 1 || ports[i] > 65535)
			throw std::runtime_error("Invalid configured listen port");
	}

	_serverConfigs.push_back(config);

	for (std::size_t i = 0; i < ports.size(); ++i)
	{
		/* Multiple server blocks may share host:port.  They are virtual hosts,
		 * so they must use one listening socket. */
		if (!hasListener(config.getHost(), ports[i]))
			_servers.push_back(ServerSocket(config, ports[i]));
	}
}

void NetworkManager::initializeServers()
{
	bool active = false;
	for (std::vector<ServerSocket>::iterator it = _servers.begin();
		 it != _servers.end(); ++it)
	{
		if (!it->create())
			throw std::runtime_error("Failed to create listening socket");

		if (!it->bindSocket())
		{
			it->closeSocket();
			throw std::runtime_error("Failed to bind listening socket");
		}

		if (!it->listenSocket())
		{
			it->closeSocket();
			throw std::runtime_error("Failed to listen on socket");
		}

		active = true;
		std::cout << "Listening on " << it->getHost()
				  << ":" << it->getPort()
				  << " (fd=" << it->getFd() << ")" << std::endl;
	}

	if (!active)
		throw std::runtime_error("No active listening sockets");
}

void NetworkManager::run()
{
	_stopRequested = 0;
	while (!_stopRequested)
	{
		buildPollFds();
		if (_pollFds.empty())
		{
			bool hasListener = false;
			for (std::size_t i = 0; i < _servers.size(); ++i)
				hasListener = hasListener || _servers[i].getFd() != -1;
			if (!hasListener && _clients.empty() && _extraFds.empty())
				throw std::runtime_error("No active listening sockets or clients");
			poll(NULL, 0, 1000);
			checkTimeouts();
			continue;
		}

		const int readyCount = poll(&_pollFds[0], _pollFds.size(), 1000);
		if (readyCount > 0)
			processEvents();

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

		for (std::map<int, ExtraFd>::const_iterator it = _extraFds.begin();
			 it != _extraFds.end(); ++it)
		{
			struct pollfd descriptor;
			descriptor.fd = it->first;
			descriptor.events = it->second.events;
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

		std::map<int, ExtraFd>::iterator extraIt = _extraFds.find(pollIt->fd);
		if (extraIt != _extraFds.end())
		{
			ExtraFd::Callback callback = extraIt->second.callback;
			void *context = extraIt->second.context;
			if (callback != NULL)
				callback(pollIt->fd, pollIt->revents, context);
			continue;
		}

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
	const int clientFd = server.acceptClient();
	if (clientFd == -1)
	{
		_listenersPausedUntil = std::time(NULL) + 2;
		shedIdleClients();
		return;
	}
	addClient(clientFd, server.getConfig(), server.getPort());
}

std::string NetworkManager::normalizeHost(const std::string& host)
{
	std::size_t start = host.find_first_not_of(" \t");
	std::size_t end = host.find_last_not_of(" \t");
	if (start == std::string::npos)
		return std::string();
	std::string result = host.substr(start, end - start + 1);
	std::size_t colon = result.find(':');
	if (colon != std::string::npos && result.find(':', colon + 1) == std::string::npos)
	{
		bool numericPort = colon + 1 < result.size();
		for (std::size_t i = colon + 1; i < result.size(); ++i)
			if (!std::isdigit(static_cast<unsigned char>(result[i])))
				numericPort = false;
		if (numericPort)
			result.erase(colon);
	}
	return result;
}

std::string NetworkManager::extractHostHeader(const std::string& raw)
{
	std::size_t lineStart = raw.find("\r\n");
	if (lineStart == std::string::npos)
		return std::string();
	lineStart += 2;
	while (lineStart < raw.size())
	{
		std::size_t lineEnd = raw.find("\r\n", lineStart);
		if (lineEnd == std::string::npos)
			return std::string();
		if (lineEnd == lineStart)
			return std::string();
		std::string line = raw.substr(lineStart, lineEnd - lineStart);
		std::size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string name = line.substr(0, colon);
			if (sameText(name, "host"))
				return normalizeHost(line.substr(colon + 1));
		}
		lineStart = lineEnd + 2;
	}
	return std::string();
}

const ServerConfig& NetworkManager::selectConfig(const std::string& listenHost, int listenPort,
												const std::string& host) const
{
	const ServerConfig *fallback = NULL;
	const std::string normalizedHost = normalizeHost(host);

	for (std::size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		const ServerConfig& config = _serverConfigs[i];
		if (config.getHost() != listenHost)
			continue;
		const std::vector<int>& ports = config.getListenPorts();
		for (std::size_t j = 0; j < ports.size(); ++j)
		{
			if (ports[j] != listenPort)
				continue;
			if (fallback == NULL)
				fallback = &config;
			if (!normalizedHost.empty()
				&& sameText(normalizedHost, config.getServerName()))
				return config;
		}
	}

	if (fallback == NULL)
		throw std::runtime_error("No server configuration for listening port");
	return *fallback;
}

void NetworkManager::handleClientRead(int clientFd)
{
	std::map<int, Client>::iterator it = _clients.find(clientFd);
	if (it == _clients.end())
		return;

	if (it->second.hasPendingResponse())
		return;

	if (!it->second.receiveData())
	{
		removeClient(clientFd);
		return;
	}

	try
	{
		if (it->second.getReadBuffer().find("\r\n\r\n") == std::string::npos)
		{
			if (it->second.getRequestError())
				sendResponse(clientFd, errorResponse(it->second.getRequestError(), it->second.getConfig()));
			return;
		}

		const std::string host = extractHostHeader(it->second.getReadBuffer());
		const ServerConfig& selected = it->second.getListenPort() == -1
			? it->second.getConfig()
			: selectConfig(it->second.getConfig().getHost(), it->second.getListenPort(), host);
		it->second.setConfig(selected);
		if (it->second.getRequestError())
		{
			sendResponse(clientFd, errorResponse(it->second.getRequestError(), selected));
			return;
		}
		if (_requestProcessor == NULL || !it->second.isRequestReady())
			return;

		std::string response;
		if (_requestProcessor(it->second.getReadBuffer(), selected, response))
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

	if (!it->second.sendData()
		|| (_requestProcessor != NULL && !it->second.hasPendingResponse()))
		removeClient(clientFd);
}

void NetworkManager::addClient(int clientFd)
{
	addClient(clientFd, ServerConfig(), -1);
}

void NetworkManager::addClient(int clientFd, const ServerConfig& config)
{
	addClient(clientFd, config, -1);
}

void NetworkManager::addClient(int clientFd, const ServerConfig& config, int listenPort)
{
	if (clientFd < 0 || _clients.find(clientFd) != _clients.end())
	{
		if (clientFd >= 0 && _clients.find(clientFd) == _clients.end())
			close(clientFd);
		return;
	}

	try
	{
		_clients.insert(std::make_pair(clientFd, Client(clientFd, config, listenPort)));
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
	{
		std::map<int, Client>::iterator clientIt = _clients.find(*it);
		if (clientIt != _clients.end())
			sendResponse(*it, errorResponse(408, clientIt->second.getConfig()));
	}

	for (std::vector<int>::const_iterator it = timedOutClients.begin();
		 it != timedOutClients.end(); ++it)
		removeClient(*it);
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
	it->second.setResponse(response);
	it->second.markRequestComplete();
	if (it->second.isClosed())
		removeClient(clientFd);
}

void NetworkManager::addExtraFd(int fd, short events, ExtraFd::Callback callback, void *context)
{
	if (fd < 0 || callback == NULL)
		return;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return;
	_extraFds[fd] = ExtraFd(fd, events, callback, context);
}

void NetworkManager::removeExtraFd(int fd)
{
	std::map<int, ExtraFd>::iterator it = _extraFds.find(fd);
	if (it != _extraFds.end())
		_extraFds.erase(it);
}

void NetworkManager::shutdown()
{
	for (std::map<int, Client>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
		it->second.closeConnection();
	_clients.clear();

	for (std::vector<ServerSocket>::iterator it = _servers.begin();
		 it != _servers.end(); ++it)
		it->closeSocket();

	_pollFds.clear();
	_extraFds.clear();
}
