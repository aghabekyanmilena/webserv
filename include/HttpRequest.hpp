#pragma once

#include <string>
#include <map>

class HttpRequest
{
private:
	std::string method;
	std::string path;
	std::string query;
	std::string version;

	std::map<std::string, std::string> headers;
	std::string body;
	std::size_t content_length;
	bool chunked;
public:
	HttpRequest();

	void setMethod(const std::string &method);
	void setPath(const std::string &path);
	void setQuery(const std::string &query);
	void setVersion(const std::string &version);
	void setHeader(const std::string &name, const std::string value);
	void setBody(const std::string &body);
	void setContentLength(size_t length);
	void setChunked(bool value);

	const std::string& getMethod() const;
	const std::string& getPath() const;
	const std::string& getQuery() const;
	const std::string& getVersion() const;
	const std::map<std::string, std::string>& getHeaders() const;
	const std::string& getBody() const;
	size_t getContentLength() const;
	bool isChunked() const;
};

// constructors and destructor