#pragma once

#include <map>
#include <string>
#include <vector>
#include "HttpTypes.hpp"

struct CgiContext
{
    std::string scriptName;
    std::string pathInfo;
    std::string scriptFilename;
    std::string serverName;
    std::string serverPort;
    std::string remoteAddr;
};

class Cgi
{
public:
    static std::vector<std::string> buildEnv(const std::string &method,
                                             const std::string &query,
                                             const std::string &body,
                                             const std::map<std::string, std::string> &headers,
                                             const CgiContext &ctx);

    static char **toEnvp(const std::vector<std::string> &env);
    static void freeEnvp(char **envp);

    static bool parseOutput(const std::string &cgiOutput, HTTPResponse &response);

private:
    static std::string headerToMeta(const std::string &name);
    static bool parseStatusValue(const std::string &value, int &statusCode);
};
