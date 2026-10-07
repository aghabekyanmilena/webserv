#include "Client.hpp"
#include "HttpParser.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <limits>
#include <cctype>

const std::size_t MAX_HEADER_SIZE = 8192;

Client::Client(int fd) : _fd(fd), _listeningPort(-1), _config(), _readBuffer(), _writeBuffer(), _lastActivity(std::time(NULL)), _requestStartedAt(_lastActivity), _headerEnd(std::string::npos),
    _bodyExpected(0), _chunkPos(0), _chunkSize(0),_decodedBodySize(0), _chunkState(0), _requestError(0), _chunked(false), _configSelected(false), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
{ }

Client::Client(int fd, const ServerConfig& config) : _fd(fd), _listeningPort(-1), _config(config), _readBuffer(), _writeBuffer(), _lastActivity(std::time(NULL)), _requestStartedAt(_lastActivity),
    _headerEnd(std::string::npos), _bodyExpected(0), _chunkPos(0), _chunkSize(0), _decodedBodySize(0), _chunkState(0), _requestError(0), _chunked(false), _configSelected(true), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
{ }

Client::Client(int fd, const ServerConfig& config, int listenPort) : _fd(fd), _listeningPort(listenPort), _config(config), _readBuffer(), _writeBuffer(), _lastActivity(std::time(NULL)),
    _requestStartedAt(_lastActivity), _headerEnd(std::string::npos), _bodyExpected(0), _chunkPos(0), _chunkSize(0), _decodedBodySize(0), _chunkState(0), _requestError(0), _chunked(false), _configSelected(false), _requestComplete(false), _requestReady(false), _responsePending(false), _closed(false)
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
    if (_requestError == 0 && _headerEnd != std::string::npos
        && (_bodyExpected > _config.getMaxBodySize()
            || _decodedBodySize > _config.getMaxBodySize()))
        _requestError = 413;
}

bool Client::receiveData()
{
    // poll->POLLIN

    char buffer[8192];
    const ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);

    if (bytesRead > 0)
    {
        try
        {
            const std::size_t oldSize = _readBuffer.size();
            _readBuffer.append(buffer, static_cast<std::string::size_type>(bytesRead));
            if (_requestReady || _requestComplete || _requestError)
            {
                update_last_activity();
                return true;
            }

            if (_headerEnd == std::string::npos)
            {
                const std::size_t searchFrom = oldSize > 3 ? oldSize - 3 : 0;
                const std::size_t end = _readBuffer.find("\r\n\r\n", searchFrom);
                if (end == std::string::npos)
                {
                    if (_readBuffer.size() > MAX_HEADER_SIZE)
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
                const HttpParser::ParseResult result = parser.parse(_readBuffer.substr(0, _headerEnd), request);
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
                _requestReady = _readBuffer.size() - _headerEnd >= _bodyExpected;
            else if (_requestError == 0)
            {
                while (_chunkPos < _readBuffer.size() && !_requestReady && !_requestError)
                {
                    if (_chunkState == 0) // chunk size line
                    {
                        const std::size_t end = _readBuffer.find("\r\n", _chunkPos);
                        if (end == std::string::npos)
                        {
                            if (_readBuffer.size() - _chunkPos > MAX_HEADER_SIZE)
                                _requestError = 400;
                            break;
                        }
                        if (end - _chunkPos > MAX_HEADER_SIZE)
                        { _requestError = 400; break; }
                        std::size_t limit = _readBuffer.find(';', _chunkPos);
                        if (limit == std::string::npos || limit > end) limit = end;
                        if (limit == _chunkPos) { _requestError = 400; break; }
                        std::size_t count = 0;
                        for (std::size_t i = _chunkPos; i < limit; ++i)
                        {
                            const unsigned char c = static_cast<unsigned char>(_readBuffer[i]);
                            if (!std::isxdigit(c)) { _requestError = 400; break; }
                            const std::size_t digit = c <= '9' ? c - '0' : std::tolower(c) - 'a' + 10;
                            if (count > (std::numeric_limits<std::size_t>::max() - digit) / 16)
                            { _requestError = 400; break; }
                            count = count * 16 + digit;
                        }
                        if (_requestError) break;
                        _chunkSize = count;
                        _chunkPos = end + 2;
                        if (count > std::numeric_limits<std::size_t>::max() - _decodedBodySize)
                        { _requestError = 400; break; }
                        if (_configSelected
                            && count > _config.getMaxBodySize() - _decodedBodySize)
                        { _requestError = 413; break; }
                        _decodedBodySize += count;
                        _chunkState = count == 0 ? 2 : 1;
                    }
                    else if (_chunkState == 1) // chunk data and trailing CRLF
                    {
                        if (_chunkSize > _readBuffer.size() - _chunkPos
                            || _readBuffer.size() - _chunkPos - _chunkSize < 2) break;
                        if (_readBuffer.compare(_chunkPos + _chunkSize, 2, "\r\n") != 0)
                        { _requestError = 400; break; }
                        _chunkPos += _chunkSize + 2;
                        _chunkState = 0;
                    }
                    else // trailer lines, ending in an empty line
                    {
                        const std::size_t end = _readBuffer.find("\r\n", _chunkPos);
                        if (end == std::string::npos)
                        {
                            if (_readBuffer.size() - _chunkPos > MAX_HEADER_SIZE)
                                _requestError = 400;
                            break;
                        }
                        if (end - _chunkPos > MAX_HEADER_SIZE)
                        { _requestError = 400; break; }
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

    /* bytesRead == 0: orderly peer shutdown.  bytesRead < 0: I/O failed.
     * We do not inspect errno after recv(), as required by the subject. */
    set_closed();
    return false;
}

bool Client::isRequestReady() const { return _requestReady; }
int Client::getRequestError() const { return _requestError; }
void Client::setRequestComplete() { _requestComplete = true; }
bool Client::isIdle() const { return !_responsePending && !_requestReady && !_requestComplete; }

const std::string& Client::getReadBuffer() const
{
    return _readBuffer;
}

void Client::clearReadBuffer()
{
    _readBuffer.clear();
}

void Client::setResponse(const std::string& response)
{
    if (_closed || response.empty())
        return;

    try
    {
        /* Append instead of replacing so a partially-sent response cannot be
         * corrupted if the application queues more output for this client. */
        _writeBuffer.append(response);
        _responsePending = !_writeBuffer.empty();
    }
    catch (...)
    {
        set_closed();
    }
}

bool Client::sendData()
{
    /* PRECONDITION: call only after poll() reports POLLOUT. */
    if (_closed)
        return false;

    if (_writeBuffer.empty())
    {
        _responsePending = false;
        return true;
    }

    const ssize_t bytesSent = send(_fd, _writeBuffer.data(), _writeBuffer.size(), 0);

    if (bytesSent > 0)
    {
        _writeBuffer.erase(0, static_cast<std::string::size_type>(bytesSent));
        _responsePending = !_writeBuffer.empty();
        update_last_activity();
        return true;
    }

    /* Do not inspect errno after send(), per the subject. */
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
    return !_requestComplete && !_requestError
        && timeoutSeconds > 0 && std::difftime(now, _requestStartedAt) >= timeoutSeconds;
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
    _readBuffer.clear();
    _writeBuffer.clear();
}
