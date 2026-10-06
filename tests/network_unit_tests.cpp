#include "Client.hpp"
#include "NetworkManager.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <csignal>

// Test-only GNU linker wrappers: the production executable is unchanged.
static bool failAllocations = false;
extern "C" void* __real__Znwm(std::size_t size);
extern "C" void* __real__Znam(std::size_t size);
extern "C" void* __wrap__Znwm(std::size_t size)
{
    if (failAllocations) throw std::bad_alloc();
    return __real__Znwm(size);
}
extern "C" void* __wrap__Znam(std::size_t size)
{
    if (failAllocations) throw std::bad_alloc();
    return __real__Znam(size);
}

static void callback(int, short, void*) {}

static void throwingCallback(int, short, void* context)
{
    ++*static_cast<int*>(context);
    throw 42; // Exercise non-std exceptions from an extra-fd callback.
}

static void stoppingCallback(int, short, void* context)
{
    assert(*static_cast<int*>(context) == 1);
    std::raise(SIGINT);
}

int main()
{
    const std::string fallback = "HTTP/1.1 408 Request Timeout\r\n"
        "Connection: close\r\nContent-Length: 0\r\n\r\n";
    Client emergency(-1);
    Client normal(-1);
    NetworkManager network;
    normal.setResponse(fallback);
    const std::time_t normalQueued = std::time(NULL);
    assert(normal.hasPendingResponse());
    assert(!normal.hasDrainTimedOut(normalQueued, 5));
    assert(normal.hasDrainTimedOut(normalQueued + 5, 5));
    assert(normal.hasPendingResponse()); // Idle timeout must not change drain state.

    int descriptors[2];
    assert(pipe(descriptors) == 0);
    failAllocations = true;
    emergency.setEmergencyResponse(fallback);
    // A failed registration returns false and keeps caller ownership.
    assert(!network.addExtraFd(descriptors[0], POLLIN, callback, NULL));
    failAllocations = false;
    const std::time_t queued = std::time(NULL);
    assert(emergency.hasPendingResponse());
    assert(!emergency.hasDrainTimedOut(queued, 5));
    assert(emergency.hasDrainTimedOut(queued + 5, 5));
    assert(close(descriptors[0]) == 0);
    assert(close(descriptors[1]) == 0);
    int first[2];
    int second[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, first) == 0);
    assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, second) == 0);
    int callbacks = 0;
    assert(network.addExtraFd(first[0], POLLIN, throwingCallback, &callbacks));
    assert(network.addExtraFd(second[0], POLLIN, stoppingCallback, &callbacks));
    // HUP supplies readiness without requiring test pipe writes.
    assert(close(first[1]) == 0 && close(second[1]) == 0);
    network.run();
    assert(callbacks == 1);
    network.shutdown();
    assert(close(first[0]) == 0 && close(second[0]) == 0);
    std::cout << "Allocation-free fallback, drain deadline, registration failure, and callback exception checks passed\n";
}
