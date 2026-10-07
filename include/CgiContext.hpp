#pragma once

#include <string>

struct CgiContext
{
    std::string query;
    std::string protocol;
    std::string remoteAddress;
    int serverPort;
    CgiContext();
};
