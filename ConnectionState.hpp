#ifndef ConnectionState_HPP
#define ConnectionState_HPP

#include "Socket.hpp"
#include "Parser.hpp"
#include <cstddef> //for the type size_t ( for unsigned sizes), it is also portable

enum class State{
    READING_HEADERS,
    READING_BODY,
    PROCESSING,
    SENDING_RESPONSE,
    ERROR,
    CLOSED
};

class ConnectionState{
public:
    explicit ConnectionState(Socket&& sock);    // Socket&& because ConnectionState is taking ownership

    State process();

    int get_fd();

    std::string_view getURI();

private:
    Socket socket_;
    char buffer_[8192];   // 8 KB stack allocated buffer, max data we can receive
    State state_;
    Http::Request request_;
    size_t offset_;    // tracks how much data we have ( using recv it may arrive in chunks )

    void parse_request();
};


#endif