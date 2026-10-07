#pragma once

#include <string>
#include <sys/stat.h>

// Copies duplicate ownership; every exceptional path still closes its descriptors.
class OwnedFd
{
    int _fd;
public:
    explicit OwnedFd(int fd = -1);
    OwnedFd(const OwnedFd& other);
    OwnedFd& operator=(const OwnedFd& other);
    ~OwnedFd();
    int get() const;
    int release();
    void reset(int fd = -1);
};

// Linux /proc/self/fd anchors pathname operations to already opened directories.
// Only the configured root may contain symlinks; every requested component uses
// O_NOFOLLOW. No realpath, readlink, openat or fcntl calls are needed.
class RootedPath
{
    OwnedFd _parent;
    OwnedFd _target;
    std::string _name;
public:
    static std::string descriptorPath(int fd);
    static int openErrorStatus(int error);
    bool resolve(const std::string& root, const std::string& relative, int& status);
    bool inspect(struct stat& info, int& status);
    std::string entryPath() const;
    int parentFd() const;
    int targetFd() const;
};
