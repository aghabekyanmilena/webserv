#pragma once

#include <string>

class Client
{
public:
    int client_fd;

    std::string input_buffer;

    std::string output_buffer;
    size_t write_offset;
};