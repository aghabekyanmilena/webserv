#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>

class Location
{
private:
	std::string path;
	std::string root;
	std::string index;

	bool autoindex;

	std::vector<std::string> methods;

	std::string redir;
	std::map<std::string, std::string> cgi;

public:
	Location();
	Location(const Location &other);
	Location &operator=(const Location &other);
	~Location();

	// getter setter
}