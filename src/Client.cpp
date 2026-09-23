#include "Client.hpp"

Client::Client(int fd) : _fd(fd) { }

Client::~Client() { }

int Client::getFd() const
{
	return _fd;
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
	int elapsed_seconds = std::difftime(now, _lastActivity);

	if (elapsed_seconds >= timeoutSeconds)
	{
        std::cout << "Timeout reached! Elapsed: " << elapsed_seconds << "s\n";
		return true;
    }
	std::cout << "Completed within time. Elapsed: " << elapsed_seconds << "s\n";
    return false;
}