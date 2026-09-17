#pragma once

#include <string>
#include <vector>

class Location
{
private:
	std::string path;
	std::string root;
	std::string index;
	bool  autoindex;

	std::vector<std::string> allowed_method;
	bool has_redir;
	int redir_code;
	std::string redir_target;
	std::string upload_directory;
public:
	Location();
	Location(const Location &other);
	Location &operator=(const Location &other);
	~Location();

	//setters
	void setPath(const std::string &path);
	void setRoot(const std::string &root);
	void setIndex(const std::string &index);
	void setAutoindex(bool value);
	void allowedMethod(const std::string &method);
	void setRedirect(int code, const std::string &target);
	void setUploadDirectory(const std::string &directory);

	//getter
	const std::string &getPath() const;
	const std::string &getRoot() const;
	const std::string &getIndex() const;
	bool getAutoindex() const;
	const std::vector<std::string> &getAllowedMethods() const;
	bool hasRedirect() const;
	int getRedirectCode() const;
	const std::string &getRedirectTarget() const;
	const std::string &getUploadDirectory() const;
}