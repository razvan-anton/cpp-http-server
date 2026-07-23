#include "TCPserver.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        uint16_t port = 8080;

        std::cout << "Initializing server on port " << port << "..." << std::endl;
        TCPserver server(port);

        std::cout<<"Starting server.."<<std::endl;
        server.start();

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}