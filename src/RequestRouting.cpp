#include "RequestRouting.hpp"

bool RequestRouting::isLocationMatch(const std::string& uri, const std::string& locationPath) const
{
    //uri = client-i tvacna, locationPath-y mer serveri configica
    if (locationPath == "/") //default root location
        return true; //location-y okaya
    if (uri == locationPath)
        return true;        
    if (uri.size() <= locationPath.size()) //"/img", "/images"
        return false;
    if (uri.compare(0, locationPath.size(), locationPath) != 0) //"/uploads/cat.txt", "/uploads"
        return false;
    return (locationPath[locationPath.size() - 1] == '/' || uri[locationPath.size()] == '/');
}

const Location* RequestRouting::findLocation(const std::string& uri, const ServerConfig& serverConfig) const
{
    const Location* bestMatch = NULL;
    std::size_t bestLength = 0;

    for (std::size_t i = 0; i < serverConfig.getLocations().size(); ++i)
    {
        const Location& location = serverConfig.getLocations()[i];
        if (isLocationMatch(uri, location.getPath()) && location.getPath().size() >= bestLength)
        {
            bestMatch = &location;
            bestLength = location.getPath().size();
        }
    }
    return bestMatch;
}

std::string RequestRouting::getRelativePath(const std::string& uri, const Location& location) const
{
    std::string relativePath = uri.substr(location.getPath().size());
    std::size_t start = relativePath.find_first_not_of('/');
    if (start == std::string::npos)
        return "";
    return relativePath.substr(start);
}

std::string RequestRouting::joinPath(const std::string& directory, const std::string& relativePath) const
{
    if (directory.empty())
        return relativePath;
    if (directory[directory.size() - 1] == '/')
        return directory + relativePath;
    return directory + "/" + relativePath;
}

std::string RequestRouting::buildFilePath(const std::string& uri, const Location& location) const
{
    return joinPath(location.getRoot(), getRelativePath(uri, location));
}
