#include "PathUtils.hpp"
#include "tracy/Tracy.hpp"

namespace File{

    
namespace fs = std::filesystem;

static std::unordered_map<std::string, std::string> build_path_cache()
{
    std::unordered_map<std::string, std::string> table;
    std::error_code ec;

    fs::path root = fs::canonical("./public", ec);
    // get full path to public
    if (ec) {
        // if error
        std::cerr << "[ERROR] Cannot find ./public directory: " << ec.message() << std::endl;
        return table;
    }

    // Iterate through all files in ./public and subdirectories
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (entry.is_regular_file()) {
            fs::path rel = fs::relative(entry.path(), root);

            std::string uri = "/" + rel.generic_string();
            // make it like a web URI

            // map every URI to its full path in a hash map
            table[uri] = entry.path().string();

            if (uri == "/index.html") {
                table["/"] = entry.path().string();
            }
        }
    }
    return table;
}

// Lookup function called on every request
// return ptr to our strings, zero allocations
const std::string* get_abs_path(std::string_view s)
{
    // Initialized exactly once on first call, never again
    // thread safe on initialisation ( because it is a local static)
    // and it is read only so thread safe even after that
    static const std::unordered_map<std::string, std::string> cache = build_path_cache();

    auto it = cache.find(std::string(s));
    if (it != cache.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t get_file_size(const int file_fd)
{
    ZoneScoped;
    int size;


    struct stat statbuf; // this will get filled by fstat with all the file's data

    if(fstat(file_fd,&statbuf)==0)
    {
        // returns 0 on success
        size=statbuf.st_size;
        return size;
    }

    return -1;
}


}