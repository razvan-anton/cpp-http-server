#ifndef TCP_server
#define TCP_server

#include "Socket.hpp"
#include <netinet/in.h>

class TCPserver
{
public:
    explicit TCPserver(uint16_t port);

    void start();


private:
    uint16_t port_;
    Socket listener_;
    struct sockaddr_in address_{};

};





#endif