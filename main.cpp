#include "TCPserver.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        // 1. Choose a port (8080 is standard for testing)
        uint16_t port = 8080;

        // 2. Instantiate the server
        std::cout << "Initializing server on port " << port << "..." << std::endl;
        TCPserver server(port);
        

        std::cout<<"Starting server.."<<std::endl;
        server.start();

    } catch (const std::exception& e) {
        // This catches your 'throw' from Socket or TCPServer
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;   //standard error
        return 1;
    }

    return 0;
}