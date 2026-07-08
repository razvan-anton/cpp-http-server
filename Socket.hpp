#ifndef SOCKET_HEADER_HPP
#define SOCKET_HEADER_HPP

#include <sys/socket.h>
#include <cstddef>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>


class Socket{
public:

    explicit Socket(int fd) : fd_(fd)    // Constructor 1
    {
        if(fd_<0)
            throw std::invalid_argument("File descriptor must be positive");
    }

    Socket() : fd_(-1){};
    // Socket() : fd_(socket(AF_INET, SOCK_STREAM, 0))   // Constructor 2
    // {
    //     if(fd_<0)
    //     {
    //         throw std::runtime_error("Failed to create socket resource"); 
    //     }
    // }
    // move this logic only for the listener so that this Constructor won't be called for every ConnState

    int get_fd() const
    {
        return fd_;
    }

    void set_non_blocking()
    {
        if (fcntl(fd_, F_SETFL, O_NONBLOCK) == -1)
        {
            throw std::runtime_error("Failed to set socket to non-blocking mode: " + std::string(std::strerror(errno)));
        }
    }

    ~Socket()       // Destructor
    {
        if (fd_>=0)
        {
            close(fd_);
        }
    }

    void deactivate_socket_without_closing()
    {
        fd_=-1;
    }

    // TO DO: de citit despre asta
    // 2. Move Constructor (Transferring ownership)
    Socket(Socket&& other) noexcept : fd_(other.fd_)  // noexcept pentru//
    {
        other.fd_=-1;
    }

    // TO DO: de citit despre asta:
    // 3. Move assignment
    Socket& operator=(Socket&& other) noexcept{
        //check to not move an object into itself
        if(this == &other)
        {
            return *this;
        }
        
        if (fd_ >= 0)
            close(fd_);

        fd_=other.fd_;

        other.fd_=-1;

        return *this;
    }


    // TO DO: de citit despre asta
    Socket(const Socket&) = delete; 
    Socket& operator=(const Socket&) = delete;
    // this prevents copies from being made of this object in all possible ways
    //therefore when the close() destructor is called, there won't be problems


private:
    int fd_;


};



#endif