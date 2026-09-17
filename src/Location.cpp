#include "../include/Location.hpp"

Location::Location() : autoindex(false), has_redir(false), redir_code(0)
{}

Location::Location(const Location &other)
	: path(other.path),
	root(other.root),
	index(other.index),
	autoindex(other.autoindex),
	allowed_method(other.allowed_method),
	has_redir(other.has_redir),
	redir_code(other.redir_code),
	redir_target(other.redir_target),
	upload_directory(other.upload_directory)
{}

Location &Location::operator=(const Location &other)
{
	if (this != &other)
	{
		path = other.path;
		root = other.root;
		index = other.index;
		autoindex = other.autoindex;
		allowed_method = other.allowed_method;
		has_redir = other.has_redir;
		redir_code = other.redir_code;
		redir_target = other.redir_target;
		upload_directory = other.upload_directory;
	}
	return *this;
}

Location::~Location() {}

void Location::setPath(const std::string &path)
{
	path = path;
}

void Location::setRoot(const std::string &root)
{
	root = root;
}

void Location::setIndex(const std::string &index)
{
	index = index;
}

void Location::setAutoindex(bool value)
{
	autoindex = value;
}

void Location::allowedMethod(const std::string &method)
{
	allowed_method.push_back(method);
}

// set redir-ic sksac