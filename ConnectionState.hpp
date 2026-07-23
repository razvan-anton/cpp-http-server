#ifndef ConnectionState_HPP
#define ConnectionState_HPP

#include <sys/epoll.h>
#include <sys/syscall.h>

#include "Socket.hpp"
#include <cstddef> //for the type size_t ( for unsigned sizes), it is also portable
#include <sys/stat.h>   // for fstat
#include <fcntl.h>  // for open
#include <sys/sendfile.h> // for sendfile
#include <mutex>


#include "PathUtils.hpp"
#include "MimeTypes.hpp"
#include "Parser.hpp"


enum class State{
    READING_HEADERS,
    READING_BODY,
    PROCESSING,
    SENDING_HEADER,
    SENDING_FILE,
    ERROR,
    CLOSED
};

class ConnectionState{
public:

    inline static int epfd; // initialised only once, set from TCPserver.cpp
    // inline: to let the linker know that all instances of epfd refer to the same variable

    explicit ConnectionState(Socket&& sock);    // Socket&& because ConnectionState is taking ownership

    ConnectionState();

    State process();

    int get_fd();

    std::string_view getURI();

    void close_gracefully();

    void reset();

    bool active();

    void init(Socket&& sock);

    void deactivate();

    void send_response();

    uint32_t get_events();

    void modify_events(uint32_t events); // sets bool to 1

    State get_state();

    std::mutex mtx;

private:
    Socket socket_;
    char buffer_[8192];   // 8 KB stack allocated buffer, max data we can receive
    State state_;
    Http::Request request_;
    size_t offset_;    // tracks how much data we have ( using recv it may arrive in chunks )
    size_t header_offset;
    size_t file_size;
    int file_fd;
    uint32_t current_events;
    bool is_active;
    off_t send_offset;

    void parse_request();
};


#endif