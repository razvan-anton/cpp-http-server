#include "TCPserver.hpp"
#include "ConnectionState.hpp"

ConnectionState connections[MAX_CONNECTIONS+5];

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

    listener_.set_non_blocking(); // set the socket to non-blocking mode,
    // so that accept() doesn't block the server if there are no incoming connections

    int epoll_fd=epoll_create(1);
    if(epoll_fd==-1)
    {
        throw std::runtime_error("Failed to create epoll instance: " + std::string(std::strerror(errno)));
    }

    struct epoll_event evlist[MAX_EVENTS]; // event list 
    // TO DO: stress test for 1024/2048 MAX_EVENTS
    struct epoll_event event; // the event we are adding  

    event.events=EPOLLIN | EPOLLET;
    // EPOLLIN for read events, EPOLLET for edge-triggered mode 
    //( more efficient (notifies us when smth changes), but we need to read until EAGAIN )
    event.data.fd=listener_.get_fd();

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listener_.get_fd(),&event)==-1) // returns -1 on error
    {
        throw std::runtime_error("Failed to add listener socket to epoll instance: " + std::string(std::strerror(errno)));
    }
    // TO DO: when making the server multi-threaded, add the EPOLLONESHOST flag
    std::cout<<"Server started"<<std::endl;
    while(true)
    {
        // logic: we process an event from the list, and if it is the listener
        // then we accept in a while loop all clients and process them

        int ready = epoll_wait(epoll_fd, evlist, MAX_EVENTS, -1); // timeout at -1 means wait until an event occurs
        // epoll_wait returns the number of events that are ready, and fills the evlist with those events
        if(ready==-1)
        {
            if(errno == EINTR) // if we were interrupted by a signal, we can just continue
            {
                continue;
            }
            throw std::runtime_error("Could not get list of ready events: " + std::string(std::strerror(errno)));
        }

        for(int i=0;i<ready;i++)
        {
            if(evlist[i].data.fd==listener_.get_fd())
            {
                // accept till EAGAIN
                while(true)
                {
                    int client_fd=accept(listener_.get_fd(),NULL,NULL);
                    // currently set to NULL cuz we don't care about the user that connects, for now
                    // it returns a new fd of the person that connected;

                    if(client_fd==-1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) //we have accepted all clients, we can break
                        {
                            break;
                        }
                        else
                        {
                            std::cerr<<"Invalid user: " + std::string(std::strerror(errno))
                                + ". Moving to the next one"<<std::endl;
                            continue;
                        }
                    }
                    else if(client_fd > MAX_CONNECTIONS)
                    {
                        std::cerr<<"Too many clients"<<std::endl;
                        // TO DO: do smth else here
                        continue;
                    }

                    process_new_client(client_fd, epoll_fd);
                }
            }
            else
            {
                //if we are here, it means that one client that we already accepted had an event
                // so we need to process it

                // get the conn state, call process, and if it is still processing, add it back to epoll
                int client_fd=evlist[i].data.fd;
                State state=connections[(client_fd)].process();
                if(state==State::PROCESSING or state==State::SENDING_FILE or state==State::SENDING_HEADER)
                {
                    connections[(client_fd)].send_response(client_fd);
                }
                if(state==State::CLOSED)
                {
                    if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,client_fd,NULL)==-1)
                    {
                        std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
                    }
                    close(client_fd);
                    std::cerr<<"Closed on client on FD: "<<client_fd<<std::endl;
                }
                else if(state==State::ERROR or state==State::CLOSED)
                {
                if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,client_fd,NULL)==-1)
                {
                    std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
                }
                    close(client_fd);
                    std::cerr<<" Error on client on FD: "<<client_fd<<std::endl;
                }
                else
                {
                    continue;
                }
            }

        }

    }
}

void TCPserver::process_new_client(const int client_fd,const int epfd)
{
    // TO DO: add a max limit of how much to read/send per client per event

    if(client_fd==-1)
    {
        std::cerr<<"Invalid user: " + std::string(std::strerror(errno))
            + ". Moving to the next one"<<std::endl;
        return;
    }


    Socket client(client_fd);
    client.set_non_blocking();  // cuz we use epoll and edge_triggered

    connections[client_fd].reset();
    connections[client_fd].init(std::move(client));

    // each user has its onw "ConnectionState" so we can have multiple users on a TCP server
    // de aici vom folosi metodele din ConnectionState pentru a lua ce avem nevoie din Request
    // dar asta dupa ce requestul va fi ok


    State state=connections[client_fd].process();

    if(state==State::PROCESSING)
    {
        //send an HTTP 200 OK
        connections[client_fd].send_response(epfd);
    }
    else if(state==State::ERROR)
    {
        if(epoll_ctl(epfd,EPOLL_CTL_DEL,client_fd,NULL)==-1)
        {
            std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
        }
        close(client_fd);
        std::cerr<<"Error on client on FD: "<<client_fd<<std::endl;
    }
    else if(state==State::CLOSED)
    {
        if(epoll_ctl(epfd,EPOLL_CTL_DEL,client_fd,NULL)==-1)
        {
            std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
        }
        close(client_fd);
        std::cerr<<"Closed client on FD: "<<client_fd<<std::endl;
    }
    
}
