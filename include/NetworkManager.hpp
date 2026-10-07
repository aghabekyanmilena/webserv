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
    struct ExtraFd
    {
        typedef void (*Callback)(int, short, void *);

        ExtraFd() : fd(-1), events(0), callback(NULL), context(NULL) {}
        ExtraFd(int value, short wantedEvents, Callback cb, void *ctx) : fd(value), events(wantedEvents), callback(cb), context(ctx) { }

        int fd;
        short events;
        Callback callback;
        void *context;
    };

    typedef bool (*RequestProcessor)(const std::string &, const ServerConfig &, std::string &);
    typedef bool (*AsyncRequestProcessor)(const std::string &, const ServerConfig &,
        std::string &, CgiRequest &, const CgiContext &);
    typedef std::string (*CgiResponseProcessor)(const std::string &, const ServerConfig &);

public:
    NetworkManager();
    ~NetworkManager();
    void setAsyncReqProcessor(AsyncRequestProcessor processor, CgiResponseProcessor responseProcessor);

    void addServer(const std::string &host, int port);
    void addServer(const ServerConfig &config);
    void initializeServers();
    void run();

    bool addExtraFd(int fd, short events, ExtraFd::Callback callback, void *context);

    void shutdown();

private:
    std::vector<ServerSocket> _servers;
    std::vector<ServerConfig> _serverConfigs;
    std::map<int, Client> _clients;
    std::vector<pollfd> _pollFds;
    std::map<int, ExtraFd> _extraFds;
    int _connectionTimeout;
    int _clearTimeout;
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

    void set_req_processor(RequestProcessor processor);

    bool has_listener(const std::string &host, int port) const;
    static std::string extract_host_header(const std::string &raw);
    static std::string normalize_host(const std::string &host);
    const ServerConfig &select_config(const std::string &listenHost, int listenPort,
                                    const std::string &host) const;
    static void handle_signal(int signalNumber);
    void send_fallback_resp(int clientFd, const std::string &response);

    void start_cgi(int clientFd, const CgiRequest &plan);
    void cancel_cgi(int clientFd);
    void refresh_cgi_fds(CgiJob &job);
    void check_cgi_jobs();
    void handle_cgi_event(int fd, short events);
    void fail_cgi_fd(int fd);
    static void cgi_callback(int fd, short events, void *context);

    const ServerConfig *get_client_config(int clientFd) const;
    void remove_extra_fd(int fd);
    void remove_free_clients();
    void remove_client(int clientFd);

    void build_poll_fds();
    void process_events();
    void handle_new_connection(ServerSocket &server);
    void handle_client_read(int clientFd);
    void handle_client_write(int clientFd);

    void add_client(int clientFd);
    void add_client(int clientFd, const ServerConfig &config);
    void add_client(int clientFd, const ServerConfig &config, int listenPort);

    void send_resp(int clientFd, const std::string &response);

    void check_timeouts();

};
