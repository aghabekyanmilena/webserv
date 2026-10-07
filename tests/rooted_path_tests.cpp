#include "RootedPath.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

static void fixture(const std::string& path, const std::string& body)
{
    std::ofstream file(path.c_str());
    file << body;
    assert(file.good());
}

static std::string contents(int pinnedFd)
{
    OwnedFd file(open(RootedPath::descriptorPath(pinnedFd).c_str(), O_RDONLY));
    assert(file.get() >= 0);
    char data[64];
    const ssize_t count = read(file.get(), data, sizeof(data));
    assert(count >= 0);
    return std::string(data, static_cast<std::size_t>(count));
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string base = argv[1];
    const std::string root = base + "/root";
    const std::string outside = base + "/outside";
    assert(mkdir(root.c_str(), 0755) == 0);
    assert(mkdir(outside.c_str(), 0755) == 0);
    assert(mkdir((root + "/nested").c_str(), 0755) == 0);
    fixture(root + "/nested/file.txt", "inside");
    fixture(outside + "/file.txt", "outside");
    assert(symlink(outside.c_str(), (root + "/escape").c_str()) == 0);
    assert(symlink((outside + "/file.txt").c_str(), (root + "/link.txt").c_str()) == 0);
    // Internal links are also denied; the configured root itself may be a link.
    assert(symlink((root + "/nested").c_str(), (root + "/internal").c_str()) == 0);
    assert(symlink(root.c_str(), (base + "/trusted-root").c_str()) == 0);
    int status = 0;
    struct stat info;
    RootedPath path;
    assert(!path.resolve(root, "escape/file.txt", status) && status == 403);
    assert(!path.resolve(root, "internal/file.txt", status) && status == 403);
    assert(path.resolve(root, "link.txt", status));
    assert(!path.inspect(info, status) && status == 403);
    assert(!path.resolve(root, "../outside/file.txt", status) && status == 403);
    assert(path.resolve(base + "/trusted-root", "nested/file.txt", status));
    assert(path.inspect(info, status) && S_ISREG(info.st_mode));
    assert(contents(path.targetFd()) == "inside");

    // Replace the final pathname after inspection: reads still use the pinned inode.
    assert(std::rename((root + "/nested/file.txt").c_str(), (root + "/nested/original.txt").c_str()) == 0);
    assert(symlink((outside + "/file.txt").c_str(), (root + "/nested/file.txt").c_str()) == 0);
    assert(contents(path.targetFd()) == "inside");
    OwnedFd copied(dup(path.targetFd()));
    assert(copied.get() >= 0);
    OwnedFd independent = copied;
    copied.reset();
    assert(contents(independent.get()) == "inside");

    RootedPath create;
    RootedPath remove;
    assert(create.resolve(root, "nested/new.txt", status));
    assert(remove.resolve(root, "nested/original.txt", status));
    // Replace the parent pathname after resolution: mutations stay in the opened directory.
    assert(std::rename((root + "/nested").c_str(), (root + "/moved").c_str()) == 0);
    assert(symlink(outside.c_str(), (root + "/nested").c_str()) == 0);
    OwnedFd uploaded(open(create.entryPath().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644));
    assert(uploaded.get() >= 0);
    assert(access((root + "/moved/new.txt").c_str(), F_OK) == 0);
    assert(access((outside + "/new.txt").c_str(), F_OK) != 0);
    assert(std::remove(remove.entryPath().c_str()) == 0);
    assert(access((root + "/moved/original.txt").c_str(), F_OK) != 0);
    assert(access((outside + "/file.txt").c_str(), F_OK) == 0);
    std::cout << "Pinned inode and parent replacement checks passed" << std::endl;
}
