#pragma once

#include <string>

// Supplied by integration; no changes to Milena's HTTPRequest type are needed.
struct CgiContext
{
    std::string query;
    std::string protocol;
    std::string remoteAddress;
    int serverPort;
    CgiContext();
};
