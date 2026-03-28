#include "Parser.hpp"

namespace Http{   // we use namespace here so we don't type Http:: 1000 times

ParseState parser(std::string_view view, Request &request)
{
    // First Space: Ends the Method.
    // Second Space: Ends the URI.
    // The first \r\n: Ends the Request Line (Version).
    // The double \r\n\r\n: Ends the Headers and marks the start of the Body.

    size_t index=view.find(' '); // returneaza indicele la primul spatiu, sau npos;
    if( index == std::string_view::npos )
    {
        if(view.size() < 15)
            return ParseState::INCOMPLETE;
        else
            return ParseState::ERROR; 
        // there should be a first space at the beggining
        //if it isn't, probably a malicious user
    }
    request.method=view.substr(0,index);

    view.remove_prefix(index+1); 
    // remove all chars up till index so that view now starts at 0
    size_t index=view.find(' ');
    if( index == std::string_view::npos )
    {
        if(view.size() < 2048) // max URI allowed length
            return ParseState::INCOMPLETE;
        else
            return ParseState::ERROR; 
    }
    request.uri=view.substr(0,index);

    view.remove_prefix(index+1); 


    size_t index=view.find("\r\n");
    if( index == std::string_view::npos or index>=15)
    {
        if(view.size() <= 12) // version should be short
            return ParseState::INCOMPLETE;
        else
            return ParseState::ERROR; 
    }

    if(view.size()<8)
        return ParseState::ERROR; // extra check so we can safely use .compare() method

    if (view.compare(0, 8, "HTTP/1.1") == 0) {  // hardcodat ca acceptam doar 2 versiuni
        request.version_major = 1;
        request.version_minor = 1;
    } else if (view.compare(0, 8, "HTTP/1.0") == 0) {  // un fel de strcmp dar nu are nevoie de NULL la final, deci mai safe aici
        request.version_major = 1;
        request.version_minor = 0;
    } else {
        return ParseState::ERROR;
    }

    view.remove_prefix(index+2);  // +2 ca acum am cautat 2 caractere, nu unul singur 

    while(true)  //true for now
    {
        size_t index=view.find("\r\n");
        if(index == std::string_view::npos)
        {
            return ParseState::INCOMPLETE;
        }
        else if(index==0) // this means we finished parsing the header ( empty new line )
        {
            if(request.content_length!=-1)
            {
                request.content_length=0; //unspecified means it is 0
                return ParseState::COMPLETE;
            }
            break;  //this means we have found the Content length 
            // in a previous iteration, we move on to read the body
        }
        else
        {
            // We parse it in this case
            // We might find the Content Length : xxx 
            // We need to see the size of body

            auto colon = view.find(':');
            if(colon != std::string_view::npos )
            {
                auto key = view.substr(0, colon);  // ce e inainte de :
                auto val = view.substr(colon + 1);   // ce e dupa :

                if(key.size()==14)  // 14=sizeof(Content-length). OBLIGATORIU ( something else would be a protocol violation )
                {
                    if(strncasecmp(key.data(), "content-length", 14) == 0)   // case insensitive check
                    {
                        // if we are here, we found the "Content-length:" in our view
                        //now we need to strip the whitespace and get the number
                    }
                }
            }




            view.remove_prefix(index+2);
        }
    }
}   

}
