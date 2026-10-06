#include "../include/Cgi.hpp"
#include <cctype>
#include <sstream>

std::string Cgi::headerToMeta(const std::string &name)
{
	std::string meta = "HTTP_";
	for (std::size_t i = 0; i < name.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(name[i]);
		if (c == '-')
			meta.push_back('_');
		else
			meta.push_back(static_cast<char>(std::toupper(c)));
	}
	return meta;
}

std::vector<std::string> Cgi::buildEnv(const std::string &method,
									   const std::string &query,
									   const std::string &body,
									   const std::map<std::string, std::string> &headers,
									   const CgiContext &ctx)
{
	std::vector<std::string> env;
	std::ostringstream contentLength;

	contentLength << body.size();
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("REQUEST_METHOD=" + method);
	env.push_back("QUERY_STRING=" + query);
	env.push_back("SCRIPT_NAME=" + ctx.scriptName);
	env.push_back("PATH_INFO=" + ctx.pathInfo);
	env.push_back("PATH_TRANSLATED=" + ctx.scriptFilename);
	env.push_back("SCRIPT_FILENAME=" + ctx.scriptFilename);
	env.push_back("SERVER_NAME=" + ctx.serverName);
	env.push_back("SERVER_PORT=" + ctx.serverPort);
	env.push_back("REMOTE_ADDR=" + ctx.remoteAddr);
	env.push_back("CONTENT_LENGTH=" + contentLength.str());

	std::map<std::string, std::string>::const_iterator contentType = headers.find("content-type");
	if (contentType == headers.end())
		contentType = headers.find("Content-Type");
	if (contentType != headers.end())
		env.push_back("CONTENT_TYPE=" + contentType->second);
	else
		env.push_back("CONTENT_TYPE=");

	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end(); ++it)
	{
		std::string lower = it->first;
		for (std::size_t i = 0; i < lower.size(); ++i)
			lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
		if (lower == "content-length" || lower == "content-type")
			continue;
		env.push_back(headerToMeta(it->first) + "=" + it->second);
	}
	return env;
}

char **Cgi::toEnvp(const std::vector<std::string> &env)
{
	char **envp = new char *[env.size() + 1];
	for (std::size_t i = 0; i < env.size(); ++i)
	{
		envp[i] = new char[env[i].size() + 1];
		for (std::size_t j = 0; j < env[i].size(); ++j)
			envp[i][j] = env[i][j];
		envp[i][env[i].size()] = '\0';
	}
	envp[env.size()] = 0;
	return envp;
}

void Cgi::freeEnvp(char **envp)
{
	if (!envp)
		return;
	for (std::size_t i = 0; envp[i] != 0; ++i)
		delete[] envp[i];
	delete[] envp;
}

bool Cgi::parseStatusValue(const std::string &value, int &statusCode)
{
	std::size_t i = 0;
	while (i < value.size() && (value[i] == ' ' || value[i] == '\t'))
		++i;
	if (i >= value.size() || value[i] < '0' || value[i] > '9')
		return false;
	int code = 0;
	while (i < value.size() && value[i] >= '0' && value[i] <= '9')
	{
		code = code * 10 + (value[i] - '0');
		++i;
	}
	if (code < 100 || code > 599)
		return false;
	statusCode = code;
	return true;
}

bool Cgi::parseOutput(const std::string &cgiOutput, HTTPResponse &response)
{
	std::size_t headerEnd = cgiOutput.find("\r\n\r\n");
	std::string sep = "\r\n";
	std::size_t sepLen = 4;
	if (headerEnd == std::string::npos)
	{
		headerEnd = cgiOutput.find("\n\n");
		sep = "\n";
		sepLen = 2;
		if (headerEnd == std::string::npos)
		{
			response.statusCode = 200;
			response.body = cgiOutput;
			response.headers["Content-Type"] = "text/plain";
			return true;
		}
	}

	const std::string headerBlock = cgiOutput.substr(0, headerEnd);
	response.body = cgiOutput.substr(headerEnd + sepLen);
	response.statusCode = 200;

	std::size_t pos = 0;
	bool hasContentType = false;
	while (pos < headerBlock.size())
	{
		std::size_t lineEnd = headerBlock.find(sep, pos);
		std::string line;
		if (lineEnd == std::string::npos)
		{
			line = headerBlock.substr(pos);
			pos = headerBlock.size();
		}
		else
		{
			line = headerBlock.substr(pos, lineEnd - pos);
			pos = lineEnd + sep.size();
		}
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		if (line.compare(0, 5, "HTTP/") == 0)
		{
			std::size_t sp1 = line.find(' ');
			if (sp1 != std::string::npos)
			{
				int code = 0;
				if (parseStatusValue(line.substr(sp1 + 1), code))
					response.statusCode = code;
			}
			continue;
		}

		std::size_t colon = line.find(':');
		if (colon == std::string::npos)
			return false;
		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		std::size_t start = value.find_first_not_of(" \t");
		if (start != std::string::npos)
			value = value.substr(start);
		else
			value = "";

		std::string lower = name;
		for (std::size_t i = 0; i < lower.size(); ++i)
			lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
		if (lower == "status")
		{
			int code = 0;
			if (!parseStatusValue(value, code))
				return false;
			response.statusCode = code;
			continue;
		}
		if (lower == "content-type")
			hasContentType = true;
		response.headers[name] = value;
	}

	if (!hasContentType && response.headers.find("Content-Type") == response.headers.end())
		response.headers["Content-Type"] = "text/plain";
	return true;
}
