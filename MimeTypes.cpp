#include "MimeTypes.hpp"
#include <unordered_map>

std::string_view MimeTypes::get_type(std::string_view path) {
    
    // very important: this is static so it is initialised ONCE, 
    //not every time the function is called
    static const std::unordered_map<std::string_view, std::string_view> mime_map = {
        {".html", "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png",  "image/png"}
    };

    size_t dot_pos = path.find_last_of('.');

    // 2. Handle the "Other" case: No dot found or dot is the last character
    if (dot_pos == std::string_view::npos || dot_pos == path.size() - 1) {
        return "application/octet-stream";
    }

    std::string_view extension = path.substr(dot_pos);

    auto it = mime_map.find(extension);
    if (it != mime_map.end()) {
        return it->second;
    }

    // for unknown extensions we will just use raw bytes
    return "application/octet-stream";
}