#pragma once

#include "CgiContext.hpp"
#include <string>
#include <vector>

struct CgiRequest
{
    std::string interpreter;
    std::string scriptPath;
    std::string body;
    std::vector<std::string> environment;

    CgiRequest();
};
