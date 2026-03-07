#include "TCPserver.hpp"


TCPserver::TCPserver(uint16_t port) :
    port_(port),
    listener_(),
    address_{}{
    // Initialize the address struct here or in start()
    address_.sin_family = AF_INET;
    address_.sin_port = htons(port_);
    address_.sin_addr.s_addr = htonl(INADDR_ANY);
}


void TCPserver::start()
{
    address_.sin_family=AF_INET;
    address_.sin_port=htons(port_);
    address_.sin_addr.s_addr=htonl(INADDR_ANY);
    if (bind(listener_.get_fd(),reinterpret_cast<struct sockaddr*>(&address_),sizeof(address_))==-1)
    {
        throw std::runtime_error("Failed to bind to port");
    }
}