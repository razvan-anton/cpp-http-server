#include "TCPserver.hpp"
#include "ConnectionState.hpp"

// TO DO: for memory efficiency, we can have a small buffer pool and store 10k smaller states
// instead of 10k big ConnectionState objects

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
    event.data.ptr=&listener_;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listener_.get_fd(),&event)==-1) // returns -1 on error
    {
        throw std::runtime_error("Failed to add listener socket to epoll instance: " + std::string(std::strerror(errno)));
    }
    // TO DO: when making the server multi-threaded, add the EPOLLONESHOST flag
    std::cout<<"Server started"<<std::endl;
    while(true)
    {
        // logic: we remove an event from the list, and if it is the listener
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
            if(evlist[i].data.ptr==&listener_)
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

                    process_client(client_fd);
                }
            }
            else
            {
                //if we are here, it means that one client that we already accepted had an event
                // so we need to process it

                // get the conn state, call process, and if it is still processing, add it back to epoll
                ConnectionState * conn = static_cast <ConnectionState*>(evlist[i].data.ptr);
                // static cast cuz .data.ptr is a void*, we need to cast it back to ConnectionState*
                // we can assign *void = smth, but not the reverse

                State state=conn->process();
                if(state==State::ERROR or state==State::CLOSED)
                {
                    std::cerr<<" Error or closed client on FD: "<<conn->get_fd()<<std::endl;
                }
                else
                {
                    continue;
                }
            }


        }




    }


}

void TCPserver::process_client(int client_fd)
{
    // TO DO: change connection state so that it recvs until EAGAIN 
    // TO DO: change when sending the file to send until EAGAIN
    // TO DO: add a max limit of how much to read/send per client per event
    if(client_fd==-1)
    {
        std::cerr<<"Invalid user: " + std::string(std::strerror(errno))
            + ". Moving to the next one"<<std::endl;
        return;
    }

{ // -> Critical: We create a scope here, so that when We are done with processing a client
    // all destructors are called so the socket closes automatically
    Socket client(client_fd);
    client.set_non_blocking();  // cuz we use epoll
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

            auto URI=Conn.getURI();
            std::optional<std::string> optional_path=File::get_abs_path(URI);
            if(!optional_path)
            {
                std::cerr<<"Wrong path given"<<std::endl;
                // PathUtils a gasit o problema la path given

                // TO DO: Logging System
                Conn.close_gracefully();

                break;
            }

            try{
                //main block, intr-un try ca pot fi multe erori aici
                // pass la *optional_path ca asa se acceseaza string-ul cand apar erori
                std::cerr << "[DEBUG] Attempting to map file..." << std::endl;

                FileMapper mapper(*optional_path);

                std::cerr << "[DEBUG] Requested Path: " << (optional_path ? *optional_path : "NULL") << std::endl;

                size_t sent=0; // ca sa dam track sa trimitem tot mesajul, also tre sa fie acelasi tip ca size
                ssize_t size=0;

                // we assemble the header: a standard 200 OK message,
                //but the Content Length needs to be the size from FileMapper
                // and Content Type is from the extension, with the help of MimeTypes class

                std::string header;
                header.reserve(256);
                header = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: " + std::string(MimeTypes::get_type(URI)) + "\r\n"
                    "Content-Length: " + std::to_string(mapper.get_size()) + "\r\n"
                    "Connection: close\r\n\r\n";

                // here we send the header first
                while(sent<header.size() and size!=-1)
                {
                    size=send(Conn.get_fd(),header.c_str()+sent,header.size()-sent,MSG_NOSIGNAL);
                    // header.c_str() e pointerul de care are nevoie functia cate primul caracter,
                    // ca sa putemm folosi std::string ; ssize_t e tipul signed al lungimii
                    // flag-ul "MSG_NOSIGNAL" ca sa nu dea SIGPIPE in caz de eroare, ci doar sa returneze 1

                    if(size<=0)
                    {
                        std::cerr<<"Temporary error while sending header on FD: "<<fd<<std::endl;
                    }
                    else if(size>0)
                        sent+=size;
                }
                
                if(size==-1)
                {
                    std::cerr<<strerror(errno)<<". Failed to send header to client on FD: "<<fd<<std::endl;
                    // folosim cerr ptc spre deosebire de cout daca avem vreo problema va afisa tot
                }
                else
                {
                    // if we are here, we have successfully sent the header
                    //now we send the body, with the same logic

                    size_t sent=0;
                    ssize_t size=0;

                    // daca nu avem body, doar va fi sarit acest while
                    while(sent<mapper.get_size() and size!=-1)
                    {
                        size=send(fd,static_cast<const uint8_t*>(mapper.get_data()) + sent,
                        mapper.get_size()-sent,MSG_NOSIGNAL);
                        //static cast entru ca data_ e void * type, trebuie char * type pt arithmetic 
                        //dar am folosit uint8_t pentru ca nu e chiar char, e raw data,
                        //plus ca asa format sa fie unsigned
                        // flag-ul "MSG_NOSIGNAL" ca sa nu dea SIGPIPE in caz de eroare, ci doar sa returneze 1

                        if(size<=0)
                        {
                            std::cerr<<"Temporary error while sending body on FD: "<<fd<<std::endl;
                        }
                        else if(size>0)
                            sent+=size;
                    }
                
                    if(size==-1)
                    {
                        std::cerr<<strerror(errno)<<". Failed to send body to client on FD: "<<fd<<std::endl;
                    }
                    else
                    {
                        std::cerr<<"Successfully sent body to client on FD: "<<fd<<std::endl;
                    }
                }

                continue;
            }
            catch(const std::exception& e){
                // daca suntem aici, a fost o problema la FileMapper
                std::cerr << "[ERROR]"<<e.what() << std::endl;

                Conn.close_gracefully();

                break;
            }



        }
        else if(state==State::READING_BODY or state==State::READING_HEADERS)
        {
            continue; // recv again, for now; TO DO
        }
        else if(state==State::ERROR or state==State::CLOSED)
        {
            std::cerr<<"Error or closed client on FD: "<<fd<<std::endl;
            break;
        }
    }
    
}
}