#pragma once

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include <signal.h>

#include "ServerSocket.hpp"
#include "Client.hpp"

class NetworkManager
{
public:
	class ExtraFd
	{
	public:
		typedef void (*Callback)(int, short, void*);
		ExtraFd() : fd(-1), events(0), callback(NULL), context(NULL) {}
		ExtraFd(int value, short wantedEvents, Callback cb, void *ctx)
			: fd(value), events(wantedEvents), callback(cb), context(ctx) {}
		int fd;
		short events;
		Callback callback;
		void *context;
	};

	typedef bool (*RequestProcessor)(const std::string&, const ServerConfig&, std::string&);

private:
	std::vector<ServerSocket> _servers;
	std::vector<ServerConfig> _serverConfigs;
	std::map<int, Client> _clients;
	std::vector<pollfd> _pollFds;
	std::map<int, ExtraFd> _extraFds;
	int _connectionTimeout;
	RequestProcessor _requestProcessor;
	static volatile sig_atomic_t _stopRequested;

	bool hasListener(const std::string& host, int port) const;
	static std::string extractHostHeader(const std::string& raw);
	static std::string normalizeHost(const std::string& host);
	const ServerConfig& selectConfig(int listenPort, const std::string& host) const;
	static void handleSignal(int signalNumber);

public:
	NetworkManager();
	~NetworkManager();
	void setRequestProcessor(RequestProcessor processor);

	void addServer(const std::string& host, int port);
	void addServer(const ServerConfig& config);
	void initializeServers();
	void run();

	void buildPollFds();
	void processEvents();
	void handleNewConnection(ServerSocket& server);
	void handleClientRead(int clientFd);
	void handleClientWrite(int clientFd);

	void addClient(int clientFd);
	void addClient(int clientFd, const ServerConfig& config);
	void addClient(int clientFd, const ServerConfig& config, int listenPort);
	void removeClient(int clientFd);

	void checkTimeouts();
	const ServerConfig* getClientConfig(int clientFd) const;
	std::string receiveRequest(int clientFd);
	void sendResponse(int clientFd, const std::string& response);

	void addExtraFd(int fd, short events, ExtraFd::Callback callback, void *context);
	void removeExtraFd(int fd);

	void shutdown();
};
