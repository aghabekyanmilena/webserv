#include "Client.hpp"
#include "HttpParser.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <limits>
#include <cctype>

static const std::size_t MAX_HEADER_SIZE = 8192;

Client::Client(int fd) : _fd(fd), _listeningPort(-1), _config(), _remoteAddr(), _readBuf(), _writeBuf(), _lastActivity(std::time(NULL)), _requestStartedAt(_lastActivity), _responseQueuedAt(0),
    _emergencyResponse(NULL), _emergencySent(0), _headerEnd(std::string::npos), _bodyExpected(0), _chunkPos(0), _chunkSize(0), _decodedBodySize(0), _chunkState(0), _requestError(0), _chunked(false),
    _configSelected(false), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
{ }

Client::Client(int fd, const ServerConfig& config) : _fd(fd), _listeningPort(-1), _config(config), _remoteAddr(), _readBuf(), _writeBuf(), _lastActivity(std::time(NULL)), _requestStartedAt(_lastActivity),
    _responseQueuedAt(0), _emergencyResponse(NULL), _emergencySent(0), _headerEnd(std::string::npos), _bodyExpected(0), _chunkPos(0), _chunkSize(0), _decodedBodySize(0), _chunkState(0), _requestError(0),
    _chunked(false), _configSelected(true), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
{ }

Client::Client(int fd, const ServerConfig& config, int listenPort) : _fd(fd), _listeningPort(listenPort), _config(config), _remoteAddr(), _readBuf(), _writeBuf(), _lastActivity(std::time(NULL)),
    _requestStartedAt(_lastActivity), _responseQueuedAt(0), _emergencyResponse(NULL), _emergencySent(0), _headerEnd(std::string::npos), _bodyExpected(0), _chunkPos(0), _chunkSize(0), _decodedBodySize(0),
    _chunkState(0), _requestError(0), _chunked(false), _configSelected(false), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
{ }

Client::~Client() { }

const ServerConfig& Client::getConfig() const
{
    return _config;
}

int Client::getFd() const
{
    return _fd;
}

int Client::getListeningPort() const
{
    return _listeningPort;
}

void Client::setConfig(const ServerConfig& config)
{
    _config = config;
    _configSelected = true;
    if (_requestError == 0 && _headerEnd != std::string::npos && (_bodyExpected > _config.getMaxBodySize() || _decodedBodySize > _config.getMaxBodySize()))
        _requestError = 413;
}

void Client::setRemoteAddress(const std::string& address)
{
    _remoteAddr = address;
}

const std::string& Client::getRemoteAddress() const
{
    return _remoteAddr;
}

bool Client::receiveData()
{
    // poll->POLLIN

    char buffer[8192];
    const ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (bytesRead > 0)
    {
        try
        {
            if (_requestComplete)
            {
                update_last_activity();
                return true;
            }
            const std::size_t oldSize = _readBuf.size();
            _readBuf.append(buffer, static_cast<std::string::size_type>(bytesRead));
            if (_requestReady || _requestComplete || _requestError)
            {
                update_last_activity();
                return true;
            }

            if (_headerEnd == std::string::npos)
            {
                const std::size_t searchFrom = oldSize > 3 ? oldSize - 3 : 0;
                const std::size_t end = _readBuf.find("\r\n\r\n", searchFrom);
                if (end == std::string::npos)
                {
                    if (_readBuf.size() > MAX_HEADER_SIZE)
                        _requestError = 431;
                    update_last_activity();
                    return true;
                }
                _headerEnd = end + 4;
                if (_headerEnd > MAX_HEADER_SIZE)
                {
                    _requestError = 431;
                    update_last_activity();
                    return true;
                }
                HttpParser parser;
                HttpRequest request;
                const HttpParser::ParseResult result = parser.parse(_readBuf.substr(0, _headerEnd), request);
                if (result == HttpParser::ERROR)
                    _requestError = 400;
                else
                {
                    _bodyExpected = request.getContentLength();
                    _chunked = request.isChunked();
                    _chunkPos = _headerEnd;
                }
            }

            if (_requestError == 0 && !_chunked)
                _requestReady = _readBuf.size() - _headerEnd >= _bodyExpected;
            else if (_requestError == 0)
            {
                while (_chunkPos < _readBuf.size() && !_requestReady && !_requestError)
                {
                    if (_chunkState == 0) // chunk size line
                    {
                        const std::size_t end = _readBuf.find("\r\n", _chunkPos);
                        if (end == std::string::npos)
                        {
                            if (_readBuf.size() - _chunkPos > MAX_HEADER_SIZE)
                                _requestError = 400;
                            break;
                        }
                        if (end - _chunkPos > MAX_HEADER_SIZE)
                        {
                            _requestError = 400;
                            break;
                        }
                        std::size_t limit = _readBuf.find(';', _chunkPos);
                        if (limit == std::string::npos || limit > end)
                            limit = end;
                        if (limit == _chunkPos)
                        {
                            _requestError = 400;
                            break;
                        }
                        std::size_t count = 0;
                        for (std::size_t i = _chunkPos; i < limit; ++i)
                        {
                            const unsigned char c = static_cast<unsigned char>(_readBuf[i]);
                            if (!std::isxdigit(c))
                            {
                                _requestError = 400;
                                break;
                            }
                            const std::size_t digit = c <= '9' ? c - '0' : std::tolower(c) - 'a' + 10;
                            if (count > (std::numeric_limits<std::size_t>::max() - digit) / 16)
                            {
                                _requestError = 400;
                                break;
                            }
                            count = count * 16 + digit;
                        }
                        if (_requestError)
                            break;
                        _chunkSize = count;
                        _chunkPos = end + 2;
                        if (count > std::numeric_limits<std::size_t>::max() - _decodedBodySize)
                        {
                            _requestError = 400;
                            break;
                        }
                        if (_configSelected && count > _config.getMaxBodySize() - _decodedBodySize)
                        {
                            _requestError = 413;
                            break;
                        }
                        _decodedBodySize += count;
                        _chunkState = count == 0 ? 2 : 1;
                    }
                    else if (_chunkState == 1) // chunk data and trailing CRLF
                    {
                        if (_chunkSize > _readBuf.size() - _chunkPos || _readBuf.size() - _chunkPos - _chunkSize < 2)
                            break;
                        if (_readBuf.compare(_chunkPos + _chunkSize, 2, "\r\n") != 0)
                        {
                            _requestError = 400;
                            break;
                        }
                        _chunkPos += _chunkSize + 2;
                        _chunkState = 0;
                    }
                    else // trailer lines, ending in an empty line
                    {
                        const std::size_t end = _readBuf.find("\r\n", _chunkPos);
                        if (end == std::string::npos)
                        {
                            if (_readBuf.size() - _chunkPos > MAX_HEADER_SIZE)
                                _requestError = 400;
                            break;
                        }
                        if (end - _chunkPos > MAX_HEADER_SIZE)
                        {
                            _requestError = 400;
                            break;
                        }
                        _requestReady = end == _chunkPos;
                        _chunkPos = end + 2;
                    }
                }
            }
        }
        catch (...)
        {
            set_closed();
            return false;
        }
        update_last_activity();
        return true;
    }

    set_closed();
    return false;
}

bool Client::isRequestReady() const { return _requestReady; }

int Client::getRequestError() const { return _requestError; }

void Client::setRequestComplete() { _requestComplete = true; }

bool Client::isIdle() const { return !_responsePending && !_requestReady && !_requestComplete; }

const std::string& Client::getReadBuffer() const
{
    return _readBuf;
}

void Client::clearReadBuffer()
{
    _readBuf.clear();
}

void Client::setResponse(const std::string& response)
{
    if (_closed || response.empty() || _emergencyResponse != NULL)
        return;

    try
    {
        if (!_responsePending)
            _responseQueuedAt = std::time(NULL);
        _writeBuf.append(response);
        _responsePending = !_writeBuf.empty();
    }
    catch (...)
    {
        set_closed();
    }
}

void Client::setEmergencyResponse(const std::string& response)
{
    if (_closed || _responsePending || response.empty())
        return;
    _emergencyResponse = &response;
    _emergencySent = 0;
    _responseQueuedAt = std::time(NULL);
    _responsePending = true;
    _requestComplete = true;
}

bool Client::sendData()
{
    // poll->POLLOUT
    if (_closed)
        return false;

    if (_emergencyResponse != NULL)
    {
        const ssize_t sent = send(_fd, _emergencyResponse->data() + _emergencySent,
            _emergencyResponse->size() - _emergencySent, MSG_DONTWAIT);
        if (sent <= 0)
        {
            set_closed();
            return false;
        }
        _emergencySent += static_cast<std::size_t>(sent);
        _responsePending = _emergencySent < _emergencyResponse->size();
        update_last_activity();
        return true;
    }

    if (_writeBuf.empty())
    {
        _responsePending = false;
        return true;
    }

    const ssize_t bytesSent = send(_fd, _writeBuf.data(), _writeBuf.size(), MSG_DONTWAIT);

    if (bytesSent > 0)
    {
        _writeBuf.erase(0, static_cast<std::string::size_type>(bytesSent));
        _responsePending = !_writeBuf.empty();
        update_last_activity();
        return true;
    }

    set_closed();
    return false;
}

bool Client::hasPendingResponse() const
{
    return _responsePending;
}

void Client::update_last_activity()
{
    _lastActivity = std::time(NULL);
}

std::time_t Client::getLastActivity() const
{
    return _lastActivity;
}

bool Client::hasTimedOut(std::time_t now, int timeoutSeconds) const
{
    if (timeoutSeconds <= 0)
        return false;

    return std::difftime(now, _lastActivity) >= timeoutSeconds;
}

bool Client::hasRequestTimedOut(std::time_t now, int timeoutSeconds) const
{
    return (!_requestComplete && !_requestError && timeoutSeconds > 0 && std::difftime(now, _requestStartedAt) >= timeoutSeconds);
}

bool Client::hasDrainTimedOut(std::time_t now, int timeoutSeconds) const
{
    return (_responsePending && timeoutSeconds > 0 && std::difftime(now, _responseQueuedAt) >= timeoutSeconds);
}

bool Client::isClosed() const
{
    return _closed;
}

void Client::set_closed()
{
    _closed = true;
}

void Client::closeConnection()
{
    if (_fd != -1)
    {
        ::shutdown(_fd, SHUT_RDWR);
        close(_fd);
        _fd = -1;
    }

    _closed = true;
    _responsePending = false;
    _emergencyResponse = NULL;
    _readBuf.clear();
    _writeBuf.clear();
}
