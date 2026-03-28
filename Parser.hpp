#ifndef PARSER
#define PARSER

#include <cstddef>   // for size_t ( unsigned )
#include <sys/types.h>  //for ssize_t ( signed size type ) -> NOT PORTABLE!!
#include <string_view>
#include <strings.h> // for case insensitive string comparations

namespace Http{
    typedef struct{
        ssize_t content_length;  // signed ca sa punem -1 pana il gasim

        std::string_view method;
        std::string_view uri;
        std::string_view body;

        int version_major;
        int version_minor;
    } Request;

    enum class ParseState{
        COMPLETE,
        INCOMPLETE,
        ERROR    // maybe erori specifice ( too large, bad request ?)
    };

    // typedef struct{
    //     Http::Request request;
    //     Http::ParseState state;
    // } ParseResult;           IDK if we still need this, for the content length

    ParseState parser(std::string_view view,Request & request);
    // parse using string_view because we don't want to modify the buffer and this prevents the slow copying
}


#endif