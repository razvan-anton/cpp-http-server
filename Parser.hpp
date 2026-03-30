#ifndef PARSER
#define PARSER

#include <cstddef>   // for size_t ( unsigned )
#include <sys/types.h>  //for ssize_t ( signed size type ) -> NOT PORTABLE!!
#include <string_view>
#include <strings.h> // for case insensitive string comparations
#include <charconv> // for std::from_chars , to convert string to int
#include <iostream> //for cout on debugging

namespace Http{
    typedef struct{
        ssize_t content_length;  // signed ca sa punem -1 pana il gasim
        size_t header_length;

        std::string_view method;
        std::string_view uri;
        std::string_view body;

        int version_major;
        int version_minor;

        void reset() 
        {
            method = {}; 
            uri = {};
            body = {};
            version_major = 0;
            version_minor = 0;
            content_length = -1;
            header_length = 0;
        }
    } Request;

    enum class ParseState{
        COMPLETE,
        INCOMPLETE,
        ERROR    // maybe erori specifice ( too large, bad request ?)
    };

    ParseState header_parser(std::string_view view,Request & request);
    // parse using string_view because we don't want to modify the buffer and this prevents the slow copying

    ParseState body_parser(std::string_view view, Request & request);
}


#endif