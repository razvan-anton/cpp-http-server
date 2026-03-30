#include "ConnectionState.hpp"


ConnectionState::ConnectionState(Socket&& sock) :
    socket_(std::move(sock)),
    buffer_{},
    state_(State::READING_HEADERS),
    request_{},
    offset_(0)
    {   request_.content_length = -1;   }; 
    //initialise to -1 so we can tell when the content has been read / set to 0

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
        if(offset_ < sizeof(buffer_))
        {
            ssize_t len=recv(socket_.get_fd(), buffer_ + offset_, sizeof(buffer_) - offset_, 0);

            //citim din socket_, bagam rez in buffer incepand de la offset,
            // citim pentru o lungime maxima de sizeof(buffer) - offset_ ( adica cat a ramas din buffer )
            // no flags
            // len e cati bytes am citit
            if(len<0)  // this means error
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) //we try again
                {
                    // cand facem epoll schimbam aici - TO DO
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

                parse_request();

                return state_;
            }
        }
        else
        { // no room left in the buffer
            std::cerr<<"Message to long. Failed to send on fd "<<socket_.get_fd()<<std::endl;
            state_=State::ERROR;
            return state_;
        }
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
                state_=State::READING_HEADERS;
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
                state_=State::PROCESSING;
                if(request_.content_length < 0)
                {
                    state_=State::ERROR;
                    break;
                }
            }               
        }
    }
}