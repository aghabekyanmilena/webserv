#pragma once

#include <string>
#include <sys/stat.h>

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