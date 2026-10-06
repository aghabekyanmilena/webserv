#pragma once

#include <vector>
#include <map>
#include <string>
#include <ctime>
#include <poll.h>
#include <signal.h>

#include "ServerSocket.hpp"
#include "Client.hpp"
#include "CgiRequest.hpp"
#include "CgiProcess.hpp"

class NetworkManager
{
public:
	class ExtraFd
	{
	public:
		typedef void (*Callback)(int, short, void *);
		ExtraFd() : fd(-1), events(0), callback(NULL), context(NULL) {}
		ExtraFd(int value, short wantedEvents, Callback cb, void *ctx)
			: fd(value), events(wantedEvents), callback(cb), context(ctx) {}
		int fd;
		short events;
		Callback callback;
		void *context;
	};

	typedef bool (*RequestProcessor)(const std::string &, const ServerConfig &, std::string &);
	typedef bool (*AsyncRequestProcessor)(const std::string &, const ServerConfig &,
		std::string &, CgiRequest &, const CgiContext &);
	typedef std::string (*CgiResponseProcessor)(const std::string &, const ServerConfig &);

private:
	std::vector<ServerSocket> _servers;
	std::vector<ServerConfig> _serverConfigs;
	std::map<int, Client> _clients;
	std::vector<pollfd> _pollFds;
	std::map<int, ExtraFd> _extraFds;
	int _connectionTimeout;
	int _drainTimeout;
	std::string _fallback408;
	std::string _fallback500;
	std::time_t _listenersPausedUntil;
	RequestProcessor _requestProcessor;
	AsyncRequestProcessor _asyncRequestProcessor;
	CgiResponseProcessor _cgiResponseProcessor;
	struct CgiJob
	{
		CgiProcess process;
		int inputFd;
		int outputFd;
		CgiJob() : inputFd(-1), outputFd(-1) {}
	};
	std::map<int, CgiJob*> _cgiJobs;
	static volatile sig_atomic_t _stopRequested;

	bool hasListener(const std::string &host, int port) const;
	static std::string extractHostHeader(const std::string &raw);
	static std::string normalizeHost(const std::string &host);
	const ServerConfig &selectConfig(const std::string &listenHost, int listenPort,
									const std::string &host) const;
	static void handleSignal(int signalNumber);
	void sendFallbackResponse(int clientFd, const std::string &response);
	void startCgi(int clientFd, const CgiRequest &plan);
	void cancelCgi(int clientFd);
	void refreshCgiFds(CgiJob &job);
	void checkCgiJobs();
	void handleCgiEvent(int fd, short events);
	void failCgiFd(int fd);
	static void cgiCallback(int fd, short events, void *context);

public:
	NetworkManager();
	~NetworkManager();
	void setRequestProcessor(RequestProcessor processor);
	void setAsyncRequestProcessor(AsyncRequestProcessor processor, CgiResponseProcessor responseProcessor);

	void addServer(const std::string &host, int port);
	void addServer(const ServerConfig &config);
	void initializeServers();
	void run();

	void buildPollFds();
	void processEvents();
	void handleNewConnection(ServerSocket &server);
	void handleClientRead(int clientFd);
	void handleClientWrite(int clientFd);

	void addClient(int clientFd);
	void addClient(int clientFd, const ServerConfig &config);
	void addClient(int clientFd, const ServerConfig &config, int listenPort);
	void removeClient(int clientFd);

	// Timeout handling
	void checkTimeouts();
	void shedIdleClients();

	// Interface toward HTTP/application layer
	// Valid until this client is removed; NULL for an unknown client.
	const ServerConfig *getClientConfig(int clientFd) const;
	std::string receiveRequest(int clientFd);
	void sendResponse(int clientFd, const std::string &response);

	// On false, ownership stays with the caller, which must close/cancel the fd.
	// Owners must use nonblocking I/O (descriptor flags or MSG_DONTWAIT).
	bool addExtraFd(int fd, short events, ExtraFd::Callback callback, void *context);
	void removeExtraFd(int fd);

	void shutdown();
};
