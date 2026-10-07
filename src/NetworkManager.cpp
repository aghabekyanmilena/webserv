#include "NetworkManager.hpp"
#include "ResponseBuilder.hpp"

#include <ctime>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <unistd.h>
#include <cctype>
#include <new>
#include <sstream>

static std::string errorResponse(int status, const ServerConfig& config)
{
#ifdef WEBSERV_FAULT_INJECT
    (void)status;
    (void)config;
    throw std::bad_alloc();
#else
    HttpResponse response = ResponseBuilder::makeError(status, "", config);
    response.setHeader("Connection", "close");
    return ResponseBuilder::serialize(response);
#endif
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

NetworkManager::NetworkManager() : _servers(), _serverConfigs(), _clients(), _pollFds(), _extraFds(), _connectionTimeout(30), _clearTimeout(5), _fallback408(), _fallback500(),
    _listenersPausedUntil(0), _requestProcessor(NULL), _asyncRequestProcessor(NULL), _cgiResponseProcessor(NULL), _cgiJobs()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, &NetworkManager::handle_signal);
    signal(SIGTERM, &NetworkManager::handle_signal);
}

NetworkManager::~NetworkManager()
{
    shutdown();
}

void NetworkManager::handle_signal(int signalNumber)
{
    if (signalNumber == SIGINT || signalNumber == SIGTERM)
        _stopRequested = 1;
}

void NetworkManager::set_req_processor(RequestProcessor processor)
{
    _requestProcessor = processor;
}

void NetworkManager::setAsyncReqProcessor(AsyncRequestProcessor processor, CgiResponseProcessor responseProcessor)
{
    _asyncRequestProcessor = processor;
    _cgiResponseProcessor = responseProcessor;
}

void NetworkManager::addServer(const std::string& host, int port)
{
    ServerConfig config;
    config.setHost(host);
    config.addListenPort(port);
    addServer(config);
}

bool NetworkManager::has_listener(const std::string& host, int port) const
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
            throw std::runtime_error("Add server configs before initialization");
    }

    const std::vector<int>& ports = config.getListeningPorts();
    if (ports.empty())
        throw std::runtime_error("Server config requires a listen port");

    for (std::size_t i = 0; i < ports.size(); ++i)
    {
        if (ports[i] < 1 || ports[i] > 65535)
            throw std::runtime_error("Invalid configured listen port");
    }

    _serverConfigs.push_back(config);

    for (std::size_t i = 0; i < ports.size(); ++i)
    {
        if (!has_listener(config.getHost(), ports[i]))
            _servers.push_back(ServerSocket(config, ports[i]));
    }
}

void NetworkManager::initializeServers()
{
    _fallback408.reserve(128);
    _fallback408 = "HTTP/1.1 408 Request Timeout\r\n"
        "Connection: close\r\nContent-Length: 0\r\n\r\n";
    _fallback500.reserve(128);
    _fallback500 = "HTTP/1.1 500 Internal Server Error\r\n"
        "Connection: close\r\nContent-Length: 0\r\n\r\n";
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
        try
        {
            build_poll_fds();
            if (_pollFds.empty())
            {
                bool has_listener = false;
                for (std::size_t i = 0; i < _servers.size(); ++i)
                    has_listener = has_listener || _servers[i].getFd() != -1;
                if (!has_listener && _clients.empty() && _extraFds.empty())
                    break;
                poll(NULL, 0, 100);
                check_cgi_jobs();
                check_timeouts();
                continue;
            }

            const int readyCount = poll(&_pollFds[0], _pollFds.size(), 1000);
            if (readyCount > 0)
                process_events();

            check_cgi_jobs();
            check_timeouts();
        }
        catch (...)
        {
            poll(NULL, 0, 100);
        }
    }
}

void NetworkManager::build_poll_fds()
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
        _pollFds.clear();
        return;
    }

    _pollFds.swap(nextPollFds);
}

void NetworkManager::process_events()
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
            try
            {
                if (pollIt->revents & POLLIN)
                    handle_new_connection(*serverIt);
            }
            catch (...)
            {
                // Failed to accept client, don't close the listener
            }
            break;
        }

        if (listenerFound)
            continue;

        try
        {
            std::map<int, ExtraFd>::iterator extraIt = _extraFds.find(pollIt->fd);
            if (extraIt != _extraFds.end())
            {
                ExtraFd::Callback callback = extraIt->second.callback;
                void *context = extraIt->second.context;
                try
                {
                    if (callback != NULL)
                        callback(pollIt->fd, pollIt->revents, context);
                }
                catch (...)
                {
                    remove_extra_fd(pollIt->fd);
                    fail_cgi_fd(pollIt->fd);
                }
                continue;
            }

            std::map<int, Client>::iterator clientIt = _clients.find(pollIt->fd);
            if (clientIt == _clients.end())
                continue;

            if (pollIt->revents & (POLLERR | POLLNVAL))
            {
                remove_client(pollIt->fd);
                continue;
            }

            if (pollIt->revents & POLLIN)
                handle_client_read(pollIt->fd);

            clientIt = _clients.find(pollIt->fd);
            if (clientIt == _clients.end())
                continue;

            if (pollIt->revents & POLLHUP)
            {
                remove_client(pollIt->fd);
                continue;
            }

            if (pollIt->revents & POLLOUT)
                handle_client_write(pollIt->fd);
        }
        catch (...)
        {
            remove_client(pollIt->fd);
        }
    }
}

void NetworkManager::handle_new_connection(ServerSocket& server)
{
    struct sockaddr_in peer;
    const int clientFd = server.acceptClient(&peer);
    if (clientFd == -1)
    {
        _listenersPausedUntil = std::time(NULL) + 2;
        remove_free_clients();
        return;
    }
    try
    {
        const unsigned long address = ntohl(peer.sin_addr.s_addr);
        std::ostringstream text;
        text << ((address >> 24) & 255) << '.' << ((address >> 16) & 255)
            << '.' << ((address >> 8) & 255) << '.' << (address & 255);
        const std::string remoteAddress = text.str();
        add_client(clientFd, server.getConfig(), server.getPort());
        std::map<int, Client>::iterator it = _clients.find(clientFd);
        if (it != _clients.end())
            it->second.setRemoteAddress(remoteAddress);
    }
    catch (...)
    {
        if (_clients.find(clientFd) != _clients.end())
            remove_client(clientFd);
        else close(clientFd);
    }
}

std::string NetworkManager::normalize_host(const std::string& host)
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
        {
            if (!std::isdigit(static_cast<unsigned char>(result[i])))
                numericPort = false;
        }
        if (numericPort)
            result.erase(colon);
    }
    return result;
}

std::string NetworkManager::extract_host_header(const std::string& raw)
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
                return normalize_host(line.substr(colon + 1));
        }
        lineStart = lineEnd + 2;
    }
    return std::string();
}

const ServerConfig& NetworkManager::select_config(const std::string& listenHost, int listenPort,
                                                const std::string& host) const
{
    const ServerConfig *fallback = NULL;
    const std::string normalizedHost = normalize_host(host);

    for (std::size_t i = 0; i < _serverConfigs.size(); ++i)
    {
        const ServerConfig& config = _serverConfigs[i];
        if (config.getHost() != listenHost)
            continue;
        const std::vector<int>& ports = config.getListeningPorts();
        for (std::size_t j = 0; j < ports.size(); ++j)
        {
            if (ports[j] != listenPort)
                continue;
            if (fallback == NULL)
                fallback = &config;
            if (!normalizedHost.empty() && sameText(normalizedHost, config.getServerName()))
                return config;
        }
    }

    if (fallback == NULL)
        throw std::runtime_error("No server configuration for listening port");
    return *fallback;
}

void NetworkManager::handle_client_read(int clientFd)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;

    if (it->second.hasPendingResponse())
        return;

    if (!it->second.receiveData())
    {
        remove_client(clientFd);
        return;
    }
    if (_cgiJobs.find(clientFd) != _cgiJobs.end())
        return;

    try
    {
        if (it->second.getReadBuffer().find("\r\n\r\n") == std::string::npos)
        {
            if (it->second.getRequestError())
                send_resp(clientFd, errorResponse(it->second.getRequestError(), it->second.getConfig()));
            return;
        }

        const std::string host = extract_host_header(it->second.getReadBuffer());
        const ServerConfig& selected = (it->second.getListeningPort() == -1) ? it->second.getConfig() : select_config(it->second.getConfig().getHost(), it->second.getListeningPort(), host);
        it->second.setConfig(selected);
        if (it->second.getRequestError())
        {
            send_resp(clientFd, errorResponse(it->second.getRequestError(), selected));
            return;
        }
        if ((_requestProcessor == NULL && _asyncRequestProcessor == NULL)
            || !it->second.isRequestReady())
            return;

        std::string response;
        CgiRequest plan;
        CgiContext context;
        context.serverPort = it->second.getListeningPort();
        context.remoteAddress = it->second.getRemoteAddress();
        const bool complete = (_asyncRequestProcessor != NULL) ? _asyncRequestProcessor(it->second.getReadBuffer(), selected, response, plan, context) : _requestProcessor(it->second.getReadBuffer(), selected, response);
        if (complete)
        {
            it->second.clearReadBuffer();
            it->second.setRequestComplete();
            if (!plan.interpreter.empty())
                start_cgi(clientFd, plan);
            else
                send_resp(clientFd, response);
        }
    }
    catch (...)
    {
        send_fallback_resp(clientFd, _fallback500);
    }
}

void NetworkManager::handle_client_write(int clientFd)
{
    try
    {
        std::map<int, Client>::iterator it = _clients.find(clientFd);
        if (it == _clients.end())
            return;

        if (!it->second.sendData() || ((_requestProcessor != NULL || _asyncRequestProcessor != NULL) && !it->second.hasPendingResponse()))
            remove_client(clientFd);
    }
    catch (...)
    {
        remove_client(clientFd);
    }
}

void NetworkManager::add_client(int clientFd)
{
    try 
    {
        add_client(clientFd, ServerConfig(), -1);
    }
    catch (...)
    {
        if (clientFd >= 0)
            close(clientFd);
    }
}

void NetworkManager::add_client(int clientFd, const ServerConfig& config)
{
    add_client(clientFd, config, -1);
}

void NetworkManager::add_client(int clientFd, const ServerConfig& config, int listenPort)
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

void NetworkManager::remove_client(int clientFd)
{
    cancel_cgi(clientFd);
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;
    for (std::size_t i = 0; i < _pollFds.size(); ++i)
    {
        if (_pollFds[i].fd == clientFd)
            _pollFds[i].revents = 0;
    }
    it->second.closeConnection();
    _clients.erase(it);
}

void NetworkManager::check_timeouts()
{
    const std::time_t now = std::time(NULL);
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); )
    {
        const int fd = it->first;
        Client& client = it->second;
        ++it;
        try
        {
            if (client.hasPendingResponse())
            {
                if (client.hasDrainTimedOut(now, _clearTimeout))
                    remove_client(fd);
            }
            else if (client.hasRequestTimedOut(now, _connectionTimeout))
            {
                try
                {
                    send_resp(fd, errorResponse(408, client.getConfig()));
                }
                catch (...)
                {
                    send_fallback_resp(fd, _fallback408);
                }
            }
            else if (client.hasTimedOut(now, _connectionTimeout))
                remove_client(fd);
        }
        catch (...)
        {
            try 
            {
                remove_client(fd);
            }
            catch (...) { }
        }
    }
}

void NetworkManager::remove_free_clients()
{
    std::map<int, Client>::const_iterator oldest = _clients.end();
    for (std::map<int, Client>::const_iterator it = _clients.begin();it != _clients.end(); ++it)
    {
        if (it->second.isIdle() && (oldest == _clients.end() || it->second.getLastActivity() < oldest->second.getLastActivity()))
            oldest = it;
    }
    if (oldest != _clients.end())
        remove_client(oldest->first);
}

const ServerConfig* NetworkManager::get_client_config(int clientFd) const
{
    std::map<int, Client>::const_iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return NULL;
    return &it->second.getConfig();
}

void NetworkManager::send_resp(int clientFd, const std::string& response)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end())
        return;
    it->second.setResponse(response);
    it->second.setRequestComplete();
    if (it->second.isClosed())
        remove_client(clientFd);
}

void NetworkManager::send_fallback_resp(int clientFd, const std::string& response)
{
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it != _clients.end())
        it->second.setEmergencyResponse(response);
}

bool NetworkManager::addExtraFd(int fd, short events, ExtraFd::Callback callback, void *context)
{
    if (fd < 0 || callback == NULL)
        return false;
    try
    {
        _extraFds[fd] = ExtraFd(fd, events, callback, context);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void NetworkManager::remove_extra_fd(int fd)
{
    for (std::size_t i = 0; i < _pollFds.size(); ++i)
    {
        if (_pollFds[i].fd == fd)
            _pollFds[i].revents = 0;
    }
    std::map<int, ExtraFd>::iterator it = _extraFds.find(fd);
    if (it != _extraFds.end())
        _extraFds.erase(it);
}

void NetworkManager::start_cgi(int clientFd, const CgiRequest& plan)
{
    std::vector<int> inheritedFds;
    for (std::size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].getFd() >= 0)
            inheritedFds.push_back(_servers[i].getFd());
    }
    for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        inheritedFds.push_back(it->first);
    }
    for (std::map<int, ExtraFd>::const_iterator it = _extraFds.begin(); it != _extraFds.end(); ++it)
    {
        inheritedFds.push_back(it->first);
    }
    CgiJob* job = new CgiJob;
    bool registered = false;
    try
    {
        registered = _cgiJobs.insert(std::make_pair(clientFd, job)).second;
        if (!registered)
        {
            delete job;
            return;
        }
        if (!job->process.start(plan.interpreter, plan.scriptPath, plan.environment, plan.body, inheritedFds, 5, plan.directory.get(), plan.script.get()))
        {
            cancel_cgi(clientFd);
            send_resp(clientFd, errorResponse(500, _clients.find(clientFd)->second.getConfig()));
            return;
        }
        job->inputFd = job->process.inputFd();
        job->outputFd = job->process.outputFd();
        if ((job->inputFd >= 0 && !addExtraFd(job->inputFd, POLLOUT, &NetworkManager::cgi_callback, this)) || !addExtraFd(job->outputFd, POLLIN, &NetworkManager::cgi_callback, this))
        {
            cancel_cgi(clientFd);
            send_fallback_resp(clientFd, _fallback500);
        }
    }
    catch (...)
    {
        if (registered)
            cancel_cgi(clientFd);
        else
            delete job;
        send_fallback_resp(clientFd, _fallback500);
    }
}

void NetworkManager::cancel_cgi(int clientFd)
{
    std::map<int, CgiJob*>::iterator it = _cgiJobs.find(clientFd);
    if (it == _cgiJobs.end())
        return;
    remove_extra_fd(it->second->inputFd);
    remove_extra_fd(it->second->outputFd);
    delete it->second;
    _cgiJobs.erase(it);
}

void NetworkManager::refresh_cgi_fds(CgiJob& job)
{
    if (job.inputFd >= 0 && job.process.inputFd() != job.inputFd)
    {
        remove_extra_fd(job.inputFd);
        job.inputFd = -1;
    }
    
    if (job.outputFd >= 0 && job.process.outputFd() != job.outputFd)
    {
        remove_extra_fd(job.outputFd);
        job.outputFd = -1;
    }
}

void NetworkManager::cgi_callback(int fd, short events, void* context)
{
    static_cast<NetworkManager*>(context)->handle_cgi_event(fd, events);
}

void NetworkManager::handle_cgi_event(int fd, short events)
{
    for (std::map<int, CgiJob*>::iterator it = _cgiJobs.begin(); it != _cgiJobs.end(); ++it)
    {
        CgiJob& job = *it->second;
        if (fd == job.inputFd)
        {
            if (events & (POLLERR | POLLHUP | POLLNVAL))
                job.process.onPipeError();
            else if (events & POLLOUT)
                job.process.onWritable();
        }
        else if (fd == job.outputFd)
        {
            if (events & (POLLERR | POLLNVAL))
                job.process.onPipeError();
            else if (events & POLLIN)
                job.process.onReadable();
            else if (events & POLLHUP)
                job.process.onOutputHangup();
        }
        else
            continue;
        refresh_cgi_fds(job);
        return;
    }
}

void NetworkManager::fail_cgi_fd(int fd)
{
    for (std::map<int, CgiJob*>::iterator it = _cgiJobs.begin(); it != _cgiJobs.end(); ++it)
    {
        if (it->second->inputFd == fd || it->second->outputFd == fd)
        {
            it->second->process.onPipeError();
            refresh_cgi_fds(*it->second);
            return;
        }
    }
}

void NetworkManager::check_cgi_jobs()
{
    const std::time_t now = std::time(NULL);
    for (std::map<int, CgiJob*>::iterator it = _cgiJobs.begin(); it != _cgiJobs.end(); )
    {
        const int clientFd = it->first;
        CgiJob& job = *it->second;
        ++it;
        try
        {
            job.process.tick(now);
            refresh_cgi_fds(job);
            if (!job.process.finished() && !job.process.errorStatus())
                continue;
            const ServerConfig* config = get_client_config(clientFd);
            if (config != NULL)
            {
                const std::string response = (job.process.errorStatus() || _cgiResponseProcessor == NULL) ? errorResponse(job.process.errorStatus() ? job.process.errorStatus() : 500, *config) : _cgiResponseProcessor(job.process.output(), *config);
                send_resp(clientFd, response);
            }
            cancel_cgi(clientFd);
        }
        catch (...)
        {
            cancel_cgi(clientFd);
            send_fallback_resp(clientFd, _fallback500);
        }
    }
    CgiProcess::reapAbandoned();
}

void NetworkManager::shutdown()
{
    while (!_cgiJobs.empty())
    {
        cancel_cgi(_cgiJobs.begin()->first);
    }
    for (int i = 0; i < 10; ++i)
    {
        CgiProcess::reapAbandoned();
        poll(NULL, 0, 10);
    }
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        it->second.closeConnection();
    }
    _clients.clear();

    for (std::vector<ServerSocket>::iterator it = _servers.begin(); it != _servers.end(); ++it)
    {
        it->closeSocket();
    }
    _pollFds.clear();
    _extraFds.clear();
}
