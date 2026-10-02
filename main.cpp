#include "NetworkManager.hpp"

#include <iostream>

int main()
{
    NetworkManager network;

    // Test multiple listening ports.
    network.addServer("0.0.0.0", 8080);
    network.addServer("0.0.0.0", 8081);

    network.initializeServers();

    std::cout << "Networking test started." << std::endl;
    std::cout << "Listening on ports 8080 and 8081." << std::endl;

    network.run();

    return 0;
}