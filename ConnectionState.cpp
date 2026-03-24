#include "ConnectionState.hpp"


ConnectionState::ConnectionState(Socket&& sock) :
    socket_(std::move(sock)),
    buffer_{},
    state_(State::READING_HEADERS),
    offset_(0){};

State ConnectionState::process()
{

}