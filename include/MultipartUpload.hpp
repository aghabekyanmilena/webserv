#pragma once
#include <string>
#include <map>

struct MultipartUpload
{
    std::string filename;
    std::string body;

    static std::string trim(const std::string& value);

    static std::string lower(std::string value);

    // Parses MIME parameters, including quoted values and quoted-pair escapes.
    static bool parameters(const std::string& text, std::string& kind,
                           std::map<std::string, std::string>& values);

    // This upload endpoint accepts exactly one file; ordinary form fields are ignored.
    bool parse(const std::string& contentType, const std::string& data);
};
