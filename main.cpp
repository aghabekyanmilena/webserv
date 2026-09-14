#include "Server.hpp"
#include "Client.hpp"

int main()
{
	Server server(8080);
	server.start();
	while (true) { };
	return 0;
}
