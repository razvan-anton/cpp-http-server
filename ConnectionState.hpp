#ifndef ConnectionState_HPP
#define ConnectionState_HPP

#include <sys/epoll.h>
#include "Socket.hpp"
#include <cstddef> //for the type size_t ( for unsigned sizes), it is also portable
#include <sys/stat.h>   // for fstat
#include <fcntl.h>  // for open
#include <sys/sendfile.h> // for sendfile


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
    explicit ConnectionState(Socket&& sock);    // Socket&& because ConnectionState is taking ownership

    ConnectionState();

    State process();

    int get_fd();

    std::string_view getURI();

    void close_gracefully();

    void reset();

    bool active();

    void init(Socket&& sock);

    void send_response(const int epfd);

private:
    Socket socket_;
    char buffer_[8192];   // 8 KB stack allocated buffer, max data we can receive
    State state_;
    Http::Request request_;
    size_t offset_;    // tracks how much data we have ( using recv it may arrive in chunks )
    bool is_active;

    size_t header_offset;
    uint32_t current_events;
    size_t file_size;
    off_t send_offset;
    int file_fd;

    void parse_request();
};


#endif