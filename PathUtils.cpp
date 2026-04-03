#include "PathUtils.hpp"

namespace File{

//
std::optional<std::string> get_abs_path(const std::string &s)
{
    try{

    std::filesystem::path root = std::filesystem::canonical("./public");
    // TO DO: save this somewhere so that there are less system calls and it is not called for every request,
    //    also maybe append the string at the full_path


    // this gives us the absolute path, from the root, regardless where the server is hosted

    std::filesystem::path request = s;
    std::filesystem::path full_path = std::filesystem::canonical(root / request);
    // here we have the asbolute path of the destination file
    // canonical makes it so it is the same on all machines, also removes the /..
    // that can be used to access unwanted directories

    auto [root_it, target_it] = std::mismatch(root.begin(), root.end(), full_path.begin(), full_path.end());
    // a filesystem object is like a vector of strings
    // the mismatch function returns the iterator of the first element
    // that is not the same in both objects

    //we check if that results is equal to root.end() to prevent any malicious users
    // and be extra safe

    //if this is true, the searched file is safely in the root path
    if (root_it == root.end())
    {
        if (std::filesystem::is_regular_file(full_path))
        {
            return full_path.string();
        }
    }
    } catch(...){  // ... means catch all
        return std::nullopt;
    }
    

    // in case the abs path is malicious/wrong/file doesn't exist
    return std::nullopt;
}

}