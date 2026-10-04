#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <istream>
#include <set>
#include <string>
#include "Config.hpp"

class ConfigParser
{
private:
    std::istream &input;
    std::string token;
    unsigned line;

    ConfigParser(const ConfigParser &other);
    ConfigParser &operator=(const ConfigParser &other);

    void fail(const std::string &message) const;
    void next();
    std::string value();
    void expect(const std::string &expected);
    size_t number(const std::string &value, size_t maximum);
    void unique(std::set<std::string> &seen, const std::string &key);
    Location location();
    ServerConfig server();

public:
    explicit ConfigParser(std::istream &stream);
    Config parse();
};

#endif
