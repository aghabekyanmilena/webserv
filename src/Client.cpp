#include "Client.hpp"

#include <sys/socket.h>
#include <unistd.h>

Client::Client(int fd)
    : _fd(fd),
      _readBuffer(),
      _writeBuffer(),
      _lastActivity(std::time(NULL)),
      _responsePending(false),
      _closed(false)
{
}

Client::~Client()
{
    /*
     * NetworkManager owns the lifetime of client file descriptors.
     * Client objects are stored by value in std::map, so closing here would
     * make temporary/copy destruction able to close a live descriptor.
     */
}

int Client::getFd() const
{
    return _fd;
}

bool Client::receiveData()
{
    /*
     * PRECONDITION: NetworkManager calls this only after poll() reports
     * POLLIN for this descriptor.  We intentionally perform one recv() per
     * readiness notification instead of reading until EAGAIN: the subject
     * forbids socket reads that were not preceded by poll() readiness and
     * forbids errno-based behaviour after read/recv.
     */
    char buffer[8192];
    const ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);

    if (bytesRead > 0)
    {
        try
        {
            _readBuffer.append(buffer, static_cast<std::string::size_type>(bytesRead));
        }
        catch (...)
        {
            markClosed();
            return false;
        }
        updateActivity();
        return true;
    }

    /* bytesRead == 0: orderly peer shutdown.  bytesRead < 0: I/O failed.
     * We do not inspect errno after recv(), as required by the subject. */
    markClosed();
    return false;
}

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
        markClosed();
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
        updateActivity();
        return true;
    }

    /* Do not inspect errno after send(), per the subject. */
    markClosed();
    return false;
}

bool Client::hasPendingResponse() const
{
    return _responsePending;
}

void Client::updateActivity()
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

bool Client::isClosed() const
{
    return _closed;
}

void Client::markClosed()
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
