#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <algorithm>

ConfigParser::ConfigParser(std::istream &stream) : input(stream), line(1) { next(); }

void ConfigParser::fail(const std::string &message) const
{
	std::ostringstream out;
	out << "Configuration line " << line << ": " << message;
	throw std::runtime_error(out.str());
}

void ConfigParser::next()
{
	token.clear();
	char c;
	while (input.get(c))
	{
		if (c == '\n')
		{
			++line;
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\r')
			continue;
		if (c == '#')
		{
			while (input.get(c) && c != '\n')
			{
			}
			if (c == '\n')
				++line;
			continue;
		}
		token += c;
		if (c == '{' || c == '}' || c == ';')
			return;
		while (input.peek() != EOF)
		{
			c = static_cast<char>(input.peek());
			if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' || c == '}' || c == ';' || c == '#')
				break;
			input.get(c);
			token += c;
		}
		return;
	}
}

std::string ConfigParser::value()
{
	if (token.empty() || token == "{" || token == "}" || token == ";")
		fail("expected directive value");
	std::string result = token;
	next();
	return result;
}

void ConfigParser::expect(const std::string &expected)
{
	if (token != expected)
		fail("expected '" + expected + "', got '" + token + "'");
	next();
}

size_t ConfigParser::number(const std::string &s, size_t maximum)
{
	size_t n = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] < '0' || s[i] > '9')
			fail("invalid number '" + s + "'");
		size_t digit = s[i] - '0';
		if (n > maximum / 10 || (n == maximum / 10 && digit > maximum % 10))
			fail("number out of range '" + s + "'");
		n = n * 10 + digit;
	}
	return n;
}

void ConfigParser::unique(std::set<std::string> &seen, const std::string &key)
{
	if (!seen.insert(key).second)
		fail("duplicate directive '" + key + "'");
}

Location ConfigParser::location()
{
	Location loc;
	std::string path = value();
	if (path[0] != '/')
		fail("location path must start with '/'");
	loc.setPath(path);
	expect("{");
	std::set<std::string> seen;
	while (token != "}")
	{
		std::string key = value();
		unique(seen, key);
		std::string arg = value();
		if (key == "root")
			loc.setRoot(arg);
		else if (key == "index")
			loc.setIndex(arg);
		else if (key == "upload_path")
			loc.setUploadDirectory(arg);
		else if (key == "cgi_extension")
		{
			if (arg[0] != '.')
				fail("cgi_extension must start with '.'");
			loc.setCgiExtension(arg);
		}
		else if (key == "cgi_path")
			loc.setCgiPath(arg);
		else if (key == "autoindex")
		{
			if (arg != "on" && arg != "off")
				fail("autoindex must be on or off");
			loc.setAutoindex(arg == "on");
		}
		else if (key == "return")
		{
			int code = static_cast<int>(number(arg, 399));
			if (code < 300)
				fail("redirect status must be 300..399");
			loc.setRedirect(code, value());
		}
		else if (key == "allowed_methods")
		{
			std::set<std::string> methods;
			while (true)
			{
				if (arg != "GET" && arg != "POST" && arg != "DELETE")
					fail("unsupported method '" + arg + "'");
				unique(methods, arg);
				loc.allowedMethod(arg);
				if (token == ";")
					break;
				arg = value();
			}
		}
		else
			fail("unknown location directive '" + key + "'");
		expect(";");
	}
	expect("}");
	if (loc.getCgiExtension().empty() != loc.getCgiPath().empty())
		fail("cgi_extension and cgi_path must be configured together");
	if (loc.getAllowedMethods().empty())
		loc.allowedMethod("GET");
	return loc;
}

ServerConfig ConfigParser::server()
{
	ServerConfig server;
	expect("server");
	expect("{");
	std::set<std::string> seen;
	std::set<std::string> paths;
	while (token != "}")
	{
		std::string key = value();
		if (key == "location")
		{
			Location loc = location();
			unique(paths, loc.getPath());
			server.addLocation(loc);
			continue;
		}
		if (key != "listen" && key != "error_page")
			unique(seen, key);
		std::string arg = value();
		if (key == "listen")
		{
			int port = static_cast<int>(number(arg, 65535));
			if (!port)
				fail("listen port must be 1..65535");
			const std::vector<int> &ports = server.getListeningPorts();
			if (std::find(ports.begin(), ports.end(), port) != ports.end())
				fail("duplicate listen port");
			server.addListenPort(port);
		}
		else if (key == "host")
			server.setHost(arg);
		else if (key == "server_name")
			server.setServerName(arg);
		else if (key == "root")
			server.setRoot(arg);
		else if (key == "client_max_body_size")
			server.setMaxBodySize(number(arg, std::numeric_limits<size_t>::max()));
		else if (key == "error_page")
		{
			int code = static_cast<int>(number(arg, 599));
			if (code < 400)
				fail("error_page status must be 400..599");
			if (server.getErrorPages().count(code))
				fail("duplicate error_page status");
			server.addErrorPage(code, value());
		}
		else
			fail("unknown server directive '" + key + "'");
		expect(";");
	}
	expect("}");
	if (server.getListeningPorts().empty())
		fail("server requires a listen directive");
	// Apply server root after the complete block, regardless of directive order.
	// Rebuild locations so callers receive their effective roots.
	ServerConfig result;
	result.setHost(server.getHost());
	result.setServerName(server.getServerName());
	result.setRoot(server.getRoot());
	result.setMaxBodySize(server.getMaxBodySize());
	for (size_t i = 0; i < server.getListeningPorts().size(); ++i)
		result.addListenPort(server.getListeningPorts()[i]);
	const std::map<int, std::string> &errors = server.getErrorPages();
	for (std::map<int, std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it)
		result.addErrorPage(it->first, it->second);
	for (size_t i = 0; i < server.getLocations().size(); ++i)
	{
		Location loc = server.getLocations()[i];
		if (loc.getRoot().empty())
			loc.setRoot(server.getRoot());
		if (loc.getRoot().empty() && !loc.hasRedirect())
			fail("location requires a root");
		result.addLocation(loc);
	}
	return result;
}

Config ConfigParser::parse()
{
	Config result;
	while (!token.empty())
		result.addServer(server());
	if (!result.validate())
		fail("configuration requires at least one server");
	return result;
}

void Config::parseFile(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file)
		throw std::runtime_error("Cannot open configuration file: " + filename);
	ConfigParser parser(file);
	Config parsed = parser.parse();
	if (file.bad())
		throw std::runtime_error("Cannot read configuration file: " + filename);
	servers = parsed.getServers();
}
