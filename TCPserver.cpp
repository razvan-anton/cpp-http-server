#include "TCPserver.hpp"
#include "ConnectionState.hpp"

ConnectionState connections[MAX_CONNECTIONS+5];

TCPserver::TCPserver(uint16_t port) :
    epoll_fd_(-1),
    port_(port),
    listener_(Socket(socket(AF_INET, SOCK_STREAM, 0))),
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

    epoll_fd_=epoll_create(1);
    if(epoll_fd_==-1)
    {
        throw std::runtime_error("Failed to create epoll instance: " + std::string(std::strerror(errno)));
    }
    ConnectionState::epfd=epoll_fd_;

    struct epoll_event evlist[MAX_EVENTS]; // event list ( interest list )
    struct epoll_event listener_event; // the event we are adding  

    listener_event.events=EPOLLIN | EPOLLET;
    listener_event.data.fd=listener_.get_fd();

    if(epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,listener_.get_fd(),&listener_event)==-1) // returns -1 on error
    {
        throw std::runtime_error("Failed to add listener socket to epoll instance: " + std::string(std::strerror(errno)));
    }
    // TO DO: when making the server multi-threaded, add the EPOLLONESHOST flag
    std::cout<<"Server started"<<std::endl;
    while(true)
    {
        // logic: we process an event from the list, and if it is the listener
        // then we accept in a while loop all clients and process them
        //std::cerr<<"DEBUG: Looking inside epoll wait"<<std::endl;
        int ready = epoll_wait(epoll_fd_, evlist, MAX_EVENTS, -1); // timeout at -1 means wait until an event occurs
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
                    //std::cerr<<"DEBUG: New Client"<<std::endl;
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

                    //fprintf(stderr, "[MAIN] 🟢 ACCEPTED fd %d\n", client_fd);

                    // logic from old process_new_client moved here

                    std::scoped_lock lock(connections[client_fd].mtx);

                    Socket client(client_fd);
                    client.set_non_blocking();  // cuz we use epoll and edge_triggered

                    connections[client_fd].reset();
                    connections[client_fd].init(std::move(client));
                    // each user has its onw "ConnectionState" so we can have multiple users on a TCP server
                    // de aici vom folosi metodele din ConnectionState pentru a lua ce avem nevoie din Request
                    // dar asta dupa ce requestul va fi ok

                    struct epoll_event event; // the event we are adding  
                    event.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
                    // EPOLLIN for read events, 
                    //EPOLLET for edge-triggered mode  
                    //( more efficient (notifies us when smth changes), but we need to read until EAGAIN )
                    // EPOLLONESHOT soo that when thread A process a client and more data arrives
                    //Thread B won't pick it up from the ready list
                    // so basically do not have two threads working the same client from different ends
                    
                    event.data.fd=client_fd;

                    if(epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,client_fd,&event)==-1)
                    {
                        // std::cerr<<"DEBUG: File descritpr that failed to be added to interest list: "<<client_fd<<std::endl;
                        // std::cerr<<"DEBUG: epoll fd: "<<epoll_fd_<<std::endl;
                        throw std::runtime_error("Failed to add new socket to epoll instance: " + std::string(std::strerror(errno)));
                    }

                }
            }
            else
            {
                //if we are here, it means that one client that we already accepted had an event
                // so we need to process it

                // get the conn state, call process, and if it is still processing, add it back to epoll
                //std::cerr<<"DEBUG: New event on already accepted client"<<std::endl;
                int client_fd=evlist[i].data.fd;

                enQ_client(client_fd);
            }

        }

    }
}

void TCPserver::process_client(const int client_fd)
{
    std::scoped_lock lock(connections[client_fd].mtx);
    State state=connections[client_fd].process();
    //std::cerr<<"DEBUG: Called .process"<<std::endl;
    if(state==State::PROCESSING or state==State::SENDING_FILE or state==State::SENDING_HEADER)
    {
        connections[client_fd].send_response();
        state=connections[client_fd].get_state();
        if((state!=State::ERROR and state!=State::CLOSED ))
        {
            //ctl_mod here
            struct epoll_event event;
            event.events=connections[client_fd].get_events();
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &event);
        }

        // if state diferit de error sau closed, ii adaugam EPOLLOUT flag
    }

    //std::cerr<<"DEBUG: State is "<<static_cast<int>(state)<<std::endl;

    if(state==State::ERROR)
    {
        connections[client_fd].deactivate();
        std::cerr<<"Error on client on FD: "<<client_fd<<std::endl;
        if(epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,client_fd,NULL)==-1)
        {
            //check for is_new cuz we can't delete something we didn;t add
            std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
        }
        // pid_t tid = syscall(SYS_gettid);
        // fprintf(stderr, "[THREAD %d] ❌ CLOSING fd %d ( error )\n", tid, client_fd);
        close(client_fd);

    }
    if(state==State::CLOSED)
    {
        connections[client_fd].deactivate();
        //std::cerr<<"DEBUG: Closing client on FD: "<<client_fd<<std::endl;
        if(epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,client_fd,NULL)==-1)
        {
            std::cerr<<"Error when deleting client on fd "<<client_fd<<std::endl;
        }
        // pid_t tid = syscall(SYS_gettid);
        // fprintf(stderr, "[THREAD %d] ❌ CLOSING fd %d ( closed normally )\n", tid, client_fd);
        close(client_fd);

    }
}

void TCPserver::enQ_client(int client_fd)
{
    pool.addTask({ [this, client_fd]() { 
        this->process_client(client_fd); 
    } });
}