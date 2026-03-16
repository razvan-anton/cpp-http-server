#ifndef SOCKET_HEADER
#define SOCKET_HEADER

#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>

class Socket{
public:

    explicit Socket(int fd) : fd_(fd)    // Constructor 1
    {
        if(fd_<0)
            throw std::invalid_argument("File descriptor must be positive");
    }

    Socket() : fd_(socket(AF_INET, SOCK_STREAM, 0))   // Constructor 2
    {
        if(fd_<0)
        {
            throw std::runtime_error("Failed to create socket resource"); 
        }
    }

    int get_fd() const
    {
        return fd_;
    }



    ~Socket()       // Destructor
    {
        if (fd_>=0)
            close(fd_);
    }


    Socket(const Socket&) = delete; 
    Socket& operator=(const Socket&) = delete;
    // this prevents copies from being made of this object in all possible ways
    //therefore when the close() destructor is called, there won't be problems


private:
    int fd_;


};



#endif