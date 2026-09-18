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

void Location::setRedirect(int code, const std::string &target)
{
	has_redir = true;
	redir_code = code;
	redir_target = target;
}

void Location::setUploadDirectory(const std::string &dir)
{
	upload_directory = dir;
}

const std::string &Location::getPath() const
{
	return path;
}

const std::string &Location::getRoot() const
{
	return root;
}

const std::string &Location::getIndex() const
{
	return index;
}

bool Location::getAutoindex() const
{
	return autoindex;
}

const std::vector<std::string> &getAllowedMethods() const
{
	return allowed_method;
}

bool Location::hasRedirect() const
{
	return has_redir;
}

int Location::getRedirectCode() const
{
	return redir_code;
}

const std::string &Location::getRedirectTarget() const
{
	return redir_target;
}

const std::string &Location::getUploadDirectory() const
{
	return upload_directory;
}