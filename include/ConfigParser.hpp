#pragma once

#include <istream>
#include <set>
#include <string>
#include <vector>
#include "ServerConfig.hpp"

class ConfigParser
{
public:
    explicit ConfigParser(std::istream &stream);

    std::vector<ServerConfig> parse();

    static std::vector<ServerConfig> parseFile(const std::string &filename);

private:
    std::istream &input;
    std::string token;
    unsigned int line;

    ConfigParser(const ConfigParser &other);

    void fail(const std::string &message) const;
    void next();
    std::string value();
    void expect(const std::string &expected);
    size_t number(const std::string &value, size_t maximum);
    void unique(std::set<std::string> &seen, const std::string &key);

    Location location();
    ServerConfig server();

    void validate(const std::vector<ServerConfig> &servers) const;
};
