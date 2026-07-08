#ifndef TCP_server_HPP
#define TCP_server_HPP

#include "Socket.hpp"


#include <string_view>
#include <netinet/in.h>
#include <optional> // for std::optional<string>

#include <sys/epoll.h> // for epoll

#define MAX_EVENTS 512 // max number of events that epoll_wait can return at once
#define MAX_CONNECTIONS 10000 // max number of clients that can be connected at once


class TCPserver
{
public:
    int epoll_fd_;

    explicit TCPserver(uint16_t port);

    void start();

    void process_new_client(const int client_fd);

    void process_client(const int client_fd);

private:
    uint16_t port_;
    Socket listener_;
    struct sockaddr_in address_{};

};


#endif