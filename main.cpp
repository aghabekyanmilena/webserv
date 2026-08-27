#include "Server.hpp"

int main()
{
	Server server(htons(8080));
	server.start();
	return 0;
}
