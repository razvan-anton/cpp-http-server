#include "PathUtils.hpp"

namespace File{

std::string get_abs_path(const std::string &s)
{
    std::filesystem::path root = "/home/razvan/Desktop/projects/CPP/http_server_cpp/public";
    std::filesystem::path request = s;
    std::filesystem::path full_path = root / request;

    if (std::filesystem::exists(full_path)) {
    if (std::filesystem::is_regular_file(full_path) and std::filesystem::canonical(full_path)==full_path) {
        // Safe to proceed. 
        // is_regular_file checks it is isn't a directory

        //  std::filesystem::canonical(full_path)==full_path)
        // checks that the path doesn't contain any /.. ( dangerous )

        return full_path.string();
    }
    }

    return "";
}

    
}