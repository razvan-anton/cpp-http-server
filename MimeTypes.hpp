#ifndef MIME_TYPES
#define MIME_TYPES

// MIME (Multipurpose Internet Mail Extensions)
// the purpose is to be able to look up in O(1) time what extension a file has and map it to its MIME Type

#include <string_view>
#include <unordered_map>

// we use class instead of namespace for portability
class MimeTypes {
public:
    // Returns the MIME type based on file extension.
    // Zero-copy: returns a view into the Data Segment.

    // static here means that we don't need a specific instance of the object to call this
    static std::string_view get_type(std::string_view path);
};



#endif