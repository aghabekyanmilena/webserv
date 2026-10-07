#pragma once

#include <string>
#include <vector>
#include <list>
#include <ctime>
#include <sys/types.h>

class CgiProcess
{
public:
    CgiProcess();
    ~CgiProcess();

    bool start(const std::string& interpreter, const std::string& scriptPath, const std::vector<std::string>& environment, const std::string& body, 
        const std::vector<int>& inheritedFds, unsigned int timeout = 5, int directoryFd = -1, int scriptFd = -1);

    int inputFd() const;
    int outputFd() const;
    int errorStatus() const;
    bool finished() const;
    const std::string& output() const;

    void onWritable();

    void onReadable();

    void onOutputHangup();
    void onPipeError();
    void cancel();

    void tick(std::time_t now);

    static void reapAbandoned(bool wait = false);

private:
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
};
