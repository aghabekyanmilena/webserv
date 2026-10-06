#pragma once

#include "RequestHandler.hpp"
#include "CgiProcess.hpp"
#include "MultipartUpload.hpp"

static void fixture(const std::string& path, const std::string& value);
static std::string readFile(const std::string& path);
static void drive(CgiProcess& process);
