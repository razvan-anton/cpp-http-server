#include "ConnectionState.hpp"

ConnectionState::ConnectionState(Socket&& sock) :
    socket_(std::move(sock)),
    buffer_{},   // TO DO: add writing to a file if a user wants to send a bigger request
    state_(State::READING_HEADERS),
    request_{},
    offset_(0),
    header_offset(0),
    file_size(-1),
    file_fd(-1),
    current_events(EPOLLIN | EPOLLET),
    is_active(1)
    {   request_.content_length = -1;   }
    //initialise to -1 so we can tell when the content has been read / set to 0

ConnectionState::ConnectionState() :
    is_active(0){}

void ConnectionState::init(Socket&& sock)
{
    socket_=(std::move(sock));
}

void ConnectionState::reset()
{
    file_fd=-1;
    file_size=-1;
    offset_=0;
    header_offset=0;
    is_active=1;
    request_.reset();
    state_=State::READING_HEADERS;
    current_events = EPOLLIN | EPOLLET;
    send_offset=0;
    //socket_=Socket(); // reset socket
}

void ConnectionState::deactivate()
{
    is_active=0;
    state_=State::CLOSED;
    close(get_fd());
    socket_.deactivate_socket_without_closing();
}


State ConnectionState::process()
{
    if(state_==State::PROCESSING)
    {
        //if we are here: this means we received one full request, and possibly more
        // we have processed the first one but it is still in the buffer
        // so we have to memmove
        
        if(offset_ >= request_.header_length + request_.content_length)
        {

            memmove(buffer_, buffer_ + (request_.header_length + request_.content_length),
            offset_ - (request_.header_length + request_.content_length));
            //mutam ce a rmas la inceputul bufferului pt a citi urm request

            offset_=offset_ - (request_.header_length + request_.content_length);
            request_.reset();
            state_=State::READING_HEADERS; // prepare to start fresh on the next read
        }

        if(offset_>0)
        {
            //this means we have some of the 2nd request remaining in the buffer
            // we must process it here, before calling recv again
           parse_request();
        }
    }

    if(state_==State::READING_HEADERS or state_==State::READING_BODY)
    {
        while(offset_ < sizeof(buffer_))
        {
            ssize_t len=recv(socket_.get_fd(), buffer_ + offset_, sizeof(buffer_) - offset_, 0);
            //citim din socket_, bagam rez in buffer incepand de la offset,
            // citim pentru o lungime maxima de sizeof(buffer) - offset_ ( adica cat a ramas din buffer )
            // no flags
            // len e cati bytes am citit
            if(len<0)  // this means error
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // that means we read all current data, and have to wait for the next edge trigger to get more
                    return state_;
                }
                else
                {
                    // if it is another error
                    std::cerr<<strerror(errno)<<". Failed to receive message on fd "<<socket_.get_fd()<<std::endl;
                    state_=State::ERROR;
                    return state_;
                }
            }
            else if(len==0)
            {
                state_=State::CLOSED;
                return state_;
                // We close it. Client doesn't send anything anymore or is very slow.
            }
            else if(len>0)
            {
                offset_+=len;
                // incepem sa ne uitam de la inceputul la bytes
                std::cerr<<"DEBUG: Read inside ConnectionState, len>0"<<std::endl;
                parse_request();

                return state_;
            }


        }

        // if we are here, there is no room left in the buffer
        std::cerr<<"Message to long. Failed to send on fd "<<socket_.get_fd()<<std::endl;
        state_=State::ERROR;
        return state_;
        
    }
    return state_; 
}

int ConnectionState::get_fd()
{
    return socket_.get_fd();
}



void ConnectionState::parse_request()
{
    while(state_==State::READING_BODY or state_==State::READING_HEADERS)
    {
        std::cerr<<"DEBUG: Parsing Request"<<std::endl;
        std::string_view view(buffer_,offset_);

        if(state_==State::READING_HEADERS)
        {
            request_.content_length=-1; // resetam pentru a fi extra safe la citirea headerului
            Http::ParseState state=Http::header_parser(view, request_);

            if (state==Http::ParseState::ERROR)
            {
                state_=State::ERROR;
            }
            else if (state == Http::ParseState::INCOMPLETE)
            {
                break;
            }
            else if (state == Http::ParseState::COMPLETE)
            {
                state_=State::READING_BODY;
                continue;

                // the next while iteration will handle all cases
            }
        }
        else if(state_==State::READING_BODY)
        {
            
            view=view.substr(request_.header_length);
            Http::ParseState state=Http::body_parser(view, request_);
            if (state==Http::ParseState::ERROR)
            {
                state_=State::ERROR;
            }
            else if(state==Http::ParseState::INCOMPLETE)
            {
                break;
            }
            else
            {
                if(request_.content_length < 0)
                {
                    state_=State::ERROR;
                    break;
                }
                // if COMPLETE:
                state_=State::PROCESSING;
                send_response();
            }               
        }
    }
}

std::string_view ConnectionState::getURI()
{
    return request_.uri;
}

void ConnectionState::close_gracefully()
{
    is_active=0;
    std::string err = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(socket_.get_fd(), err.c_str(), err.size(), MSG_NOSIGNAL);

    shutdown(socket_.get_fd(), SHUT_WR); 
    // The Kernel sends a FIN (Finish) packet to the client

    // (consume any remaining bytes from client)
    char junk[1024];
    while(recv(socket_.get_fd(), junk, sizeof(junk), MSG_DONTWAIT) > 0);
    // flag-ul MSG_DONTWAIT e ca sa trimita tot ce are acum in buffer, 
    // si sa nu stea sa astepte dupa client deloc
}


bool ConnectionState::active()
{
    return is_active;
}

void ConnectionState::send_response()
{
    if( state_ == State::PROCESSING)
        state_=State::SENDING_HEADER;
    // DEBUG: ADDED THIS

    if( state_ == State::SENDING_HEADER)
    {
        auto URI = this->getURI();
        std::optional<std::string> optional_path=File::get_abs_path(URI);
        if(!optional_path)
        {
            std::cerr<<"Wrong path given"<<std::endl;
            // PathUtils a gasit o problema la path given

            // TO DO: Logging System
            this->close_gracefully();
            return;
        }

        if(file_fd==-1)
        {
            file_fd=open(optional_path->c_str(),O_RDONLY);
            //open gives us the fd
            if(file_fd==-1)
            {
                throw std::system_error(errno, std::generic_category(), "Failed to open file");
            }
        }

        file_size=File::get_file_size(file_fd);

        if(file_size<=0)
        {
            std::cerr<<"File size is 0 or negative"<<std::endl;
            this->close_gracefully();
            return;
        }


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
            "Content-Length: " + std::to_string(file_size) + "\r\n"
            "Connection: close\r\n\r\n";

        // here we send the header first
        while(sent<header.size() and size!=-1)
        {
            size=send(this->get_fd(),header.c_str()+sent,header.size()-sent,MSG_NOSIGNAL);
            // header.c_str() e pointerul de care are nevoie functia cate primul caracter,
            // ca sa putemm folosi std::string ; ssize_t e tipul signed al lungimii
            // flag-ul "MSG_NOSIGNAL" ca sa nu dea SIGPIPE in caz de eroare, ci doar sa returneze 1

            if(size>0)
                sent+=size;
        }

        if(sent==header.size())
        {
            std::cerr<<"DEBUG: header Sent to client"<<std::endl;
            state_=State::SENDING_FILE;
            header_offset=0;
        }


        if(size==-1)
        {
            if(errno == EAGAIN or errno == EWOULDBLOCK)
            {
                // if we are here this emans that the kernel send buffer is full and we need to come back to this
                // from the epoll wait list when we get an EPOLLOUT SIGNAL

                std::cerr<<"DEBUG: Send buffer is full. Add to wait list and come back later"<<std::endl;

                header_offset=sent;
                if(current_events!= (EPOLLIN | EPOLLET | EPOLLOUT) and file_size > 0)
                {
                    struct epoll_event event;
                    event.events=EPOLLIN | EPOLLET | EPOLLOUT;
                    current_events=EPOLLIN | EPOLLET | EPOLLOUT;
                    event.data.fd=this->get_fd();
                    epoll_ctl(epfd, EPOLL_CTL_MOD, this->get_fd(), &event);
                }
                return;

            }
            else
            {
                std::cerr<<strerror(errno)<<". Failed to send header to client on FD: "<<this->get_fd()<<std::endl;
            }
        }
    }

    
    if(state_==State::SENDING_FILE)
    {

        size_t sent=0;
        ssize_t size=0;

        // daca nu avem body, doar va fi sarit acest while
        std::cerr<<"DEBUG: send_offset, file_size: "<<send_offset<<" "<<file_size<<std::endl;
        while((size_t)send_offset<file_size and size!=-1)
        {
            size=sendfile(this->get_fd(),file_fd,&send_offset,file_size - (size_t)send_offset);
            if(size>0)
                sent+=size;

            if(size==0)
                break;
        }

        if(sent==file_size)
        {
            state_=State::CLOSED;
            //change flags in case we might get a second connection; ( only if they send Connection: keep-alive)
            if(current_events==(EPOLLIN | EPOLLET | EPOLLOUT))
            {
                struct epoll_event event;
                event.events=EPOLLIN | EPOLLET;
                current_events=EPOLLIN | EPOLLET;
                event.data.fd=this->get_fd();
                epoll_ctl(epfd, EPOLL_CTL_MOD, this->get_fd(), &event);
                // cuz we finished the sending; we don't care about out events anymore
            }
            std::cerr<<"DEBUG: File sent"<<std::endl;
            return;
        }

        if(size==-1)
        {
            if(errno == EAGAIN or errno == EWOULDBLOCK)
            {
                // if we are here this emans that the kernel send buffer is full and we need to come back to this
                // from the epoll wait list when we get an EPOLLOUT SIGNAL
                std::cerr<<"DEBUG: Send buffer is full. Add to wait list and come back later to send file"<<std::endl;
                if(current_events!=(EPOLLIN | EPOLLET | EPOLLOUT))
                {
                    struct epoll_event event;
                    event.events=EPOLLIN | EPOLLET | EPOLLOUT;
                    current_events=EPOLLIN | EPOLLET | EPOLLOUT;
                    event.data.fd=this->get_fd();
                    epoll_ctl(epfd, EPOLL_CTL_MOD, this->get_fd(), &event);
                }
                return;

            }
            else
            {
                state_=State::ERROR;
                std::cerr<<strerror(errno)<<". Failed to send file to client on FD: "<<this->get_fd()<<std::endl;
                std::cerr<<"DEBUG: Client fd is "<<this->get_fd()<<", file fd is "<<file_fd<<" and file size is "<<file_size<<std::endl;
            }
        }

    }

}