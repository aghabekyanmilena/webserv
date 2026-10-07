#include "CgiProcess.hpp"
#include "RootedPath.hpp"
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

CgiProcess::Child::Child() : pid(-1), abandoned(false)
{
}

CgiProcess::Children& CgiProcess::children()
{ static Children value; return value; }

void CgiProcess::closeFd(int& fd)
{ if (fd >= 0) close(fd); fd = -1; }

void CgiProcess::fail(int status)
{
    if (_error == 0) _error = status;
    closeFd(_inputFd);
    closeFd(_outputFd);
    if (_pid > 0) kill(_pid, SIGKILL);
}

CgiProcess::CgiProcess()
    : _pid(-1), _inputFd(-1), _outputFd(-1), _sent(0),
      _outputLimit(8 * 1024 * 1024), _started(0), _timeout(5),
      _error(0), _exitStatus(0), _startedOnce(false)
{}

CgiProcess::~CgiProcess()
{
    cancel();
    tick(std::time(NULL));
    if (_pid > 0) _child->abandoned = true;
}

bool CgiProcess::start(const std::string& interpreter, const std::string& scriptPath,
           const std::vector<std::string>& environment, const std::string& body,
           const std::vector<int>& inheritedFds, unsigned int timeout,
           int directoryFd, int scriptFd)
{
    if (_startedOnce) return false;
    _startedOnce = true;
    if (interpreter.empty() || interpreter[0] != '/' || timeout == 0)
    { _error = 500; return false; }
    _timeout = timeout;
    _input = body;
    const std::size_t slash = scriptPath.rfind('/');
    std::string directory = slash == std::string::npos ? "." :
        (slash == 0 ? "/" : scriptPath.substr(0, slash));
    std::string script = slash == std::string::npos ? scriptPath : scriptPath.substr(slash + 1);
    if (script.empty()) { _error = 500; return false; }
    if (interpreter.find('\0') != std::string::npos || scriptPath.find('\0') != std::string::npos)
    { _error = 500; return false; }
    for (std::size_t i = 0; i < environment.size(); ++i)
        if (environment[i].empty() || environment[i].find('\0') != std::string::npos)
        { _error = 500; return false; }
    script = "./" + script; // A filename beginning with a dash must not become an interpreter option.
    if (directoryFd >= 0 || scriptFd >= 0)
    {
        if (directoryFd <= STDERR_FILENO || scriptFd <= STDERR_FILENO)
        { _error = 500; return false; }
        directory = RootedPath::descriptorPath(directoryFd);
        script = RootedPath::descriptorPath(scriptFd);
    }
    std::vector<std::string> envCopy(environment);
    std::vector<char*> env;
    for (std::size_t i = 0; i < envCopy.size(); ++i) env.push_back(&envCopy[i][0]);
    env.push_back(NULL);
    std::string executable = interpreter;
    char* args[] = { &executable[0], &script[0], NULL };
    // Allocate bookkeeping before fork so parent-side allocation cannot orphan a child.
    _child = children().insert(children().end(), Child());
    int inputSockets[2] = { -1, -1 };
    int outputSockets[2] = { -1, -1 };
    // The child needs blocking stdin/stdout. Only the server endpoints use
    // MSG_DONTWAIT, so no descriptor flag changes or unlisted pipe2() are needed.
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, inputSockets) < 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, outputSockets) < 0)
    {
        closeFd(inputSockets[0]); closeFd(inputSockets[1]);
        closeFd(outputSockets[0]); closeFd(outputSockets[1]);
        children().erase(_child); _error = 500; return false;
    }
    if (inputSockets[0] <= STDERR_FILENO || inputSockets[1] <= STDERR_FILENO ||
        outputSockets[0] <= STDERR_FILENO || outputSockets[1] <= STDERR_FILENO)
    {
        closeFd(inputSockets[0]); closeFd(inputSockets[1]);
        closeFd(outputSockets[0]); closeFd(outputSockets[1]);
        children().erase(_child); _error = 500; return false;
    }
    _pid = fork();
    if (_pid == 0)
    {
        signal(SIGPIPE, SIG_DFL);
        if (dup2(inputSockets[0], STDIN_FILENO) < 0 ||
            dup2(outputSockets[1], STDOUT_FILENO) < 0 ||
            chdir(directory.c_str()) < 0)
            std::exit(EXIT_FAILURE);
        const int channelFds[] = { inputSockets[0], inputSockets[1], outputSockets[0], outputSockets[1] };
        for (std::size_t i = 0; i < 4; ++i)
            if (channelFds[i] > STDERR_FILENO) close(channelFds[i]);
        for (std::size_t i = 0; i < inheritedFds.size(); ++i)
            if (inheritedFds[i] > STDERR_FILENO && inheritedFds[i] != scriptFd)
                close(inheritedFds[i]);
        if (directoryFd > STDERR_FILENO) close(directoryFd);
        execve(executable.c_str(), args, &env[0]);
        std::exit(EXIT_FAILURE);
    }
    closeFd(inputSockets[0]); closeFd(outputSockets[1]);
    if (_pid < 0)
    {
        closeFd(inputSockets[1]); closeFd(outputSockets[0]);
        children().erase(_child); _error = 500; return false;
    }
    _child->pid = _pid;
    _inputFd = inputSockets[1]; _outputFd = outputSockets[0];
    _started = std::time(NULL);
    if (_input.empty()) closeFd(_inputFd); // CGI gets EOF, even for an empty body.
    return true;
}

int CgiProcess::inputFd() const
{ return _inputFd; }

int CgiProcess::outputFd() const
{ return _outputFd; }

int CgiProcess::errorStatus() const
{ return _error; }

bool CgiProcess::finished() const
{ return _startedOnce && _pid < 0 && _outputFd < 0; }

const std::string& CgiProcess::output() const
{ return _output; }

void CgiProcess::onWritable()
{
    if (_inputFd < 0 || _error != 0) return;
    const std::size_t remaining = _input.size() - _sent;
    const std::size_t count = remaining < 8192 ? remaining : 8192;
    const ssize_t written = send(_inputFd, _input.data() + _sent, count, MSG_DONTWAIT);
    if (written <= 0) { fail(500); return; } // Never inspect errno after I/O.
    _sent += static_cast<std::size_t>(written);
    if (_sent == _input.size()) closeFd(_inputFd);
}

void CgiProcess::onReadable()
{
    if (_outputFd < 0 || _error != 0) return;
    char buffer[8192];
    const ssize_t received = recv(_outputFd, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (received < 0) { fail(500); return; }
    if (received == 0) { closeFd(_outputFd); return; }
    const std::size_t count = static_cast<std::size_t>(received);
    if (count > _outputLimit - _output.size()) { fail(500); return; }
    try { _output.append(buffer, count); }
    catch (...) { fail(500); }
}

void CgiProcess::onOutputHangup()
{ closeFd(_outputFd); }

void CgiProcess::onPipeError()
{ fail(500); }

void CgiProcess::cancel()
{ if (_pid > 0 || _inputFd >= 0 || _outputFd >= 0) fail(500); }

void CgiProcess::tick(std::time_t now)
{
    if (_pid > 0)
    {
        const pid_t result = waitpid(_pid, &_exitStatus, WNOHANG);
        if (result == _pid)
        {
            children().erase(_child); _pid = -1; closeFd(_inputFd);
            if (!WIFEXITED(_exitStatus) || WEXITSTATUS(_exitStatus) != 0) fail(500);
        }
    }
    if (_error == 0 && (_pid > 0 || _outputFd >= 0) &&
        std::difftime(now, _started) >= _timeout) fail(504);
    reapAbandoned();
}

void CgiProcess::reapAbandoned()
{
    Children& pending = children();
    for (Children::iterator it = pending.begin(); it != pending.end(); )
    {
        int status = 0;
        if (it->abandoned && waitpid(it->pid, &status, WNOHANG) == it->pid)
            it = pending.erase(it);
        else ++it;
    }
}
