#include "RootedPath.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <stdexcept>

OwnedFd::OwnedFd(int fd) : _fd(fd) {}
OwnedFd::OwnedFd(const OwnedFd& other) : _fd(-1)
{
    if (other._fd >= 0)
    {
        _fd = dup(other._fd);
        if (_fd < 0) throw std::runtime_error("Cannot duplicate file descriptor");
    }
}
OwnedFd& OwnedFd::operator=(const OwnedFd& other)
{
    if (this != &other)
    {
        const int replacement = other._fd < 0 ? -1 : dup(other._fd);
        if (other._fd >= 0 && replacement < 0)
            throw std::runtime_error("Cannot duplicate file descriptor");
        reset(replacement);
    }
    return *this;
}
OwnedFd::~OwnedFd() { reset(); }
int OwnedFd::get() const { return _fd; }
int OwnedFd::release() { const int fd = _fd; _fd = -1; return fd; }
void OwnedFd::reset(int fd)
{
    if (_fd >= 0) close(_fd);
    _fd = fd;
}

std::string RootedPath::descriptorPath(int fd)
{
    std::ostringstream path;
    path << "/proc/self/fd/" << fd;
    return path.str();
}

int RootedPath::openErrorStatus(int error)
{
    if (error == EACCES || error == EPERM || error == ELOOP) return 403;
    if (error == ENOENT || error == ENOTDIR) return 404;
    return 500;
}

bool RootedPath::resolve(const std::string& root, const std::string& relative, int& status)
{
    _target.reset();
    _parent.reset();
    _name.clear();
    status = 403;
    if (root.empty() || (!relative.empty() && relative[0] == '/')) return false;
    // Validate before opening: indices and multipart filenames use this path too.
    for (std::size_t i = 0; i < relative.size(); ++i)
        if (static_cast<unsigned char>(relative[i]) < 32 || relative[i] == 127) return false;
    std::size_t start = 0;
    while (start < relative.size())
    {
        const std::size_t end = relative.find('/', start);
        if (relative.substr(start, end == std::string::npos ? end : end - start) == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    _parent.reset(open(root.c_str(), O_PATH | O_DIRECTORY));
    if (_parent.get() < 0) { status = openErrorStatus(errno); return false; }
    start = 0;
    while (start < relative.size())
    {
        const std::size_t end = relative.find('/', start);
        const std::string component = relative.substr(start, end == std::string::npos ? end : end - start);
        if (end == std::string::npos)
        { _name = component; break; }
        if (!component.empty() && component != ".")
        {
            const std::string nextPath = descriptorPath(_parent.get()) + "/" + component;
            OwnedFd next(open(nextPath.c_str(), O_PATH | O_NOFOLLOW));
            if (next.get() < 0) { status = openErrorStatus(errno); return false; }
            struct stat info;
            if (stat(descriptorPath(next.get()).c_str(), &info) != 0)
            { status = 500; return false; }
            if (S_ISLNK(info.st_mode)) { status = 403; return false; }
            if (!S_ISDIR(info.st_mode)) { status = 404; return false; }
            _parent = next;
        }
        start = end + 1;
    }
    if (_name.empty()) _name = ".";
    status = 200;
    return true;
}

bool RootedPath::inspect(struct stat& info, int& status)
{
    _target.reset(open(entryPath().c_str(), O_PATH | O_NOFOLLOW));
    if (_target.get() < 0) { status = openErrorStatus(errno); return false; }
    if (stat(descriptorPath(_target.get()).c_str(), &info) != 0)
    { status = 500; return false; }
    if (S_ISLNK(info.st_mode)) { status = 403; return false; }
    status = 200;
    return true;
}

std::string RootedPath::entryPath() const { return descriptorPath(_parent.get()) + "/" + _name; }
int RootedPath::parentFd() const { return _parent.get(); }
int RootedPath::targetFd() const { return _target.get(); }
