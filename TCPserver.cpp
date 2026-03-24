#include "TCPserver.hpp"
#include "ConnectionState.hpp"


TCPserver::TCPserver(uint16_t port) :
    port_(port),
    listener_(),
    address_{}{
    // Initialize the address struct here or in start(), with all 0's
    //htons converts the data to Network byte order ( from Little Endian to Big Endian )
    address_.sin_family = AF_INET;
    address_.sin_port = htons(port_);
    address_.sin_addr.s_addr = htonl(INADDR_ANY);
}


void TCPserver::start()
{
    if (bind(listener_.get_fd(),reinterpret_cast<struct sockaddr*>(&address_),sizeof(address_))==-1)
    // reinterpret cast so that the C++ compiler can read that struct without warnings/errors
    {
        throw std::runtime_error("Failed to bind to port: " + std::string(std::strerror(errno)));
    }
    if(listen(listener_.get_fd(),SOMAXCONN)==-1)
    // listen to the port in order to get connections;
    //SOMAXCONN is the max number of people that can be queued to listen ( hardcoded at 4096 )
    {
        throw std::runtime_error("Failed to listen to port" + std::string(std::strerror(errno)));
    }
    std::cout<<"Server started"<<std::endl;
    while(true)
    {
        int fd2=accept(listener_.get_fd(),NULL,NULL);
        // currently set to NULL cuz we don't care about the user that connects, for now
        // it returns a new fd of the person that connected;

        if(fd2==-1)
        {
            std::cerr<<"Invalid user: " + std::string(std::strerror(errno))
             + ". Moving to the next one"<<std::endl;
            continue;
        }

        // Socket client(fd2); //create a new socket for the user
        // std::cerr<<"New connection succeeded on FD: "<<fd2<<std::endl;
        // V1
        Socket client(fd2);
        ConnectionState Conn(client);
        
        









        // std::string msg="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
        // //correct format for a HTTP message

        // ssize_t size=send(client.get_fd(),msg.c_str(),msg.size(),0);
        // V1

        //msg.c_str() e pointerul de care are nevoie functia cate primul caracter,
        //ca sa putemm folosi std::string ; ssize_t e tipul signed al lungimii
        // for now, setam flag ul la 0


        //aici verificam: daca nu s a trimis deloc
        //sau daca am trimis mai putini bytes decat avem in mesaj
        // (send reutnreaza cati bytes a trimis)
        //folosim cerr ptc spre deosebire de cout daca avem vreo problema va afisa tot
        // if(size==-1)
        // {
        //     std::cerr<<strerror(errno)<<". Failed to send message to client on FD: "<<fd2<<std::endl;
        // }
        // else if( static_cast<size_t>(size) < msg.size() ) //C++ way to typecast
        // {
        //     std::cerr<<"Failed to send full message to client on FD: "<<fd2<<std::endl;
        // }
        // else
        // {
        //     std::cerr<<"Successfully sent message to client on FD: "<<fd2<<std::endl;
        // }
        //V1
    }


}