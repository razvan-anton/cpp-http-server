#include "TCPserver.hpp"
#include "ConnectionState.hpp"


TCPserver::TCPserver(uint16_t port) :
    port_(port),
    listener_(),
    address_{}{
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
        ConnectionState Conn(std::move(client));
        // each user has its onw "ConnectionState" so we can have multiple users on a TCP server
        // de aici vom folosi metodele din ConnectionState pentru a lua ce avem nevoie din Request
        // dar asta dupa ce requestul va fi ok
        
        while(true)
        {
            State state=Conn.process();


            int fd=Conn.get_fd();
            if(state==State::PROCESSING)
            {
                //send an HTTP 200 OK
                state=State::SENDING_RESPONSE;

                std::string msg="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"; 
                //correct format for a HTTP message
 
                size_t sent=0; // ca sa dam track sa trimitem tot mesajul, also tre sa fie acelasi tip ca size
                ssize_t size=0;
                while(sent<msg.size() and size!=-1)
                {
                    size=send(fd,msg.c_str()+sent,msg.size()-sent,0);
                    // msg.c_str() e pointerul de care are nevoie functia cate primul caracter,
                    // ca sa putemm folosi std::string ; ssize_t e tipul signed al lungimii
                    // for now, setam flag ul la 0
                    if(size<=0)
                    {
                        std::cerr<<"Temporary error while sending message on FD: "<<fd<<std::endl;
                    }
                    else if(size>0)
                        sent+=size;
                }
                
                if(size==-1)
                {
                    std::cerr<<strerror(errno)<<". Failed to send message to client on FD: "<<fd<<std::endl;
                    // folosim cerr ptc spre deosebire de cout daca avem vreo problema va afisa tot
                }
                else
                {
                    std::cerr<<"Successfully sent message to client on FD: "<<fd<<std::endl;
                }

                continue;
            }
            else if(state==State::READING_BODY or state==State::READING_HEADERS)
            {
                continue; // recv again, for now
            }
            else if(state==State::ERROR or state==State::CLOSED)
            {
                std::cerr<<"Error or closed client on FD: "<<fd<<std::endl;
                break;
            }
        }

    }


}