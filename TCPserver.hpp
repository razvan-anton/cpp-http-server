#ifndef TCP_server_HPP
#define TCP_server_HPP

#include "Socket.hpp"
#include "PathUtils.hpp"
#include "FileMapper.hpp"
#include "MimeTypes.hpp"

#include <string_view>
#include <netinet/in.h>
#include <optional> // for std::optional<string>


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