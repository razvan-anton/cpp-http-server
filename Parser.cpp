#include "Parser.hpp"

namespace Http{   // we use namespace here so we don't type Http:: 1000 times

ParseState header_parser(std::string_view view, Request &request)
{
    // First Space: Ends the Method.
    // Second Space: Ends the URI.
    // The first \r\n: Ends the Request Line (Version).
    // The double \r\n\r\n: Ends the Headers and marks the start of the Body.

    // TO DO: folosim un enum ca sa dam skip la ce avem completat

    // TO DO: support \n as valid endline ( although it is not HTTP compliant )

    int cursor=0; //for total header length

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

    cursor+=(index+1);
    index=view.find(' ');
    if( index == std::string_view::npos )
    {
        if(view.size() < 2048) // max URI allowed length
            return ParseState::INCOMPLETE;
        else
        {
            std::cerr<<"Error: Could not find a proper URI"<<std::endl;
            return ParseState::ERROR; 
        }
    }

    request.uri=view.substr(0,index);
    view.remove_prefix(index+1); 
    cursor+=(index+1);


    index=view.find("\r\n");
    
    if( index == std::string_view::npos or index>=15)
    {
        if(view.size() <= 12) // version should be short
            return ParseState::INCOMPLETE;
        else
        {
            
            // std::cerr << "ERR_VIEW_START |";
            // for(char c : view) {
            //     if (c == '\r') std::cerr << "\\r";
            //     else if (c == '\n') std::cerr << "\\n";
            //     else std::cerr << c;
            // }
            // std::cerr << "| ERR_VIEW_END" << std::endl;
            //DEBUG

            std::cerr<<"Error: Version length too big"<<std::endl;
            return ParseState::ERROR; 
        }

    }

    if(view.size()<8)
    {
        std::cerr<<"Error: Version too short"<<std::endl;
        return ParseState::ERROR; // extra check so we can safely use .compare() method
    }

    if (view.compare(0, 8, "HTTP/1.1") == 0) {  // hardcodat ca acceptam doar 2 versiuni
        request.version_major = 1;
        request.version_minor = 1;
    } else if (view.compare(0, 8, "HTTP/1.0") == 0) {  // un fel de strcmp dar nu are nevoie de NULL la final, deci mai safe aici
        request.version_major = 1;
        request.version_minor = 0;
    } else {
        std::cerr<<"Error: Could not find a proper version"<<std::endl;
        return ParseState::ERROR;
    }

    view.remove_prefix(index+2);  // +2 ca acum am cautat 2 caractere, nu unul singur 
    cursor+=(index+2);

    while(true)  //true for now
    {
        index=view.find("\r\n");

        if(index == std::string_view::npos)
        {
            return ParseState::INCOMPLETE;
        }
        else if(index==0) // this means we finished parsing the header ( empty new line )
        {
            if(request.content_length==-1)
            {
                request.content_length=0; //unspecified means it is 0

            }
            cursor+=2;
            request.header_length=cursor;
            return ParseState::COMPLETE;
            
        }
        else
        {
            // We parse it in this case
            // We might find the Content Length : xxx 
            // We need to see the size of body

            auto colon = view.find(':');
            if(colon== std::string_view::npos or index < colon)
            {
                return ParseState::ERROR;
            }
            if(colon != std::string_view::npos )
            {
                auto key = view.substr(0, colon);  // ce e inainte de :
                auto val = view.substr(colon + 1, index - (colon + 1));   // ce e dupa : , pana la \r\n

                if(key.size()==14)  // 14=sizeof(Content-length). OBLIGATORIU ( something else would be a protocol violation )
                {
                    if(strncasecmp(key.data(), "content-length", 14) == 0)   // case insensitive check
                    {
                        
                        // if we are here, we found the "Content-length:" in our view

                        if(request.content_length!=-1)
                            return ParseState::ERROR;
                        // this means we already found a previous COntent-length. There can only be one

                        //now we need to strip the whitespace and get the number
                        val.remove_prefix(std::min(val.find_first_not_of(" \t"), val.size()));
                        val.remove_suffix(val.size() - (val.find_last_not_of(" \t") + 1));
                        // \t e tab
                        // we use min for the case in which we don't find any whitespace
                        std::from_chars_result res = std::from_chars(val.data(), val.data() + val.size(), request.content_length);
                        // .data() e ptr spre inceputul sirului
                        //res e o structura specifica care are si erori

                        if (res.ec == std::errc::invalid_argument) {
                            return ParseState::ERROR; // Not a number (e.g., "abc")
                        }

                        if (res.ec == std::errc::result_out_of_range) {
                            return ParseState::ERROR; // Number too big for ssize_t
                        }

                        // res.ptr e pointeru unde s a terminat numarul
                        if (res.ptr != val.data() + val.size()) {
                            return ParseState::ERROR; // Trailing junk detected (e.g., "100ab")
                        }
                    }
                }
            } 
            view.remove_prefix(index+2);
            cursor+=(index+2);
        }
    }
}   

ParseState body_parser(std::string_view view, Request & request)
{
    if(request.content_length < 0)
    {
        return ParseState::ERROR;
    }
    size_t body_len=static_cast<size_t>(request.content_length);
    // cast for the comparison
    if(view.size()<body_len)
    {
        return ParseState::INCOMPLETE;
    }
    else
    {
        request.body=view.substr(0,request.content_length);
        return ParseState::COMPLETE;
        // citim cat ne trebuie noua, restul lasam ca sa verificam dupa in ConnectionState 
        // daca mai e ceva din alt Request
    }
}


}
