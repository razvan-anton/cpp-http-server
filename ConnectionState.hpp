#ifndef ConnectionState_HPP
#define ConnectionState_HPP

#include "Socket.hpp"
#include <cstddef> //for the type size_t ( for unsigned sizes)

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

private:
    Socket socket_;
    State state_;

    size_t offset_;    // tracks how much data we have ( using recv it may arrive in chunks )
    char buffer_[8192];   // 8 KB stack allocated buffer, max data we can receive
};


#endif