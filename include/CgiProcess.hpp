#pragma once

#include <string>
#include <vector>
#include <list>
#include <ctime>
#include <sys/types.h>

// No poll call lives here: the networking layer supplies readiness events.
class CgiProcess
{
    struct Child
    {
        pid_t pid;
        bool abandoned;
        Child();
    };
    typedef std::list<Child> Children;
    static Children& children();
    Children::iterator _child;
    pid_t _pid;
    int _inputFd;
    int _outputFd;
    std::string _input;
    std::string _output;
    std::size_t _sent;
    std::size_t _outputLimit;
    std::time_t _started;
    unsigned int _timeout;
    int _error;
    int _exitStatus;
    bool _startedOnce;

    CgiProcess(const CgiProcess&);
    CgiProcess& operator=(const CgiProcess&);
    static void closeFd(int& fd);
    void fail(int status);

public:
    CgiProcess();

    ~CgiProcess();

    // interpreter must be absolute; scriptPath is relative to the server cwd.
    // inheritedFds must include all server/client/other-CGI descriptors.
    // The server supplies pinned directory/script descriptors. The child uses
    // them for its cwd and interpreter argument, retaining the script across exec.
    bool start(const std::string& interpreter, const std::string& scriptPath,
               const std::vector<std::string>& environment, const std::string& body,
               const std::vector<int>& inheritedFds, unsigned int timeout = 5,
               int directoryFd = -1, int scriptFd = -1);

    int inputFd() const;
    int outputFd() const;
    int errorStatus() const;
    bool finished() const;
    const std::string& output() const;

    // Call only after the SAME server poll loop reports POLLOUT for inputFd().
    void onWritable();

    // Call only after POLLIN for outputFd(); drain HUP only when POLLIN also appears.
    void onReadable();

    // POLLHUP without POLLIN means the output channel has no more buffered bytes.
    void onOutputHangup();
    void onPipeError();
    void cancel();

    // Call on EVERY event-loop iteration, including poll timeouts.
    void tick(std::time_t now);

    // Also call when no active CGI jobs remain; never waits for a running child.
    static void reapAbandoned();
};
