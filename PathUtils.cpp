#include "PathUtils.hpp"

namespace File{

//
std::optional<std::string> get_abs_path(std::string_view &s)
{
    

    static const std::filesystem::path root = std::filesystem::canonical("./public");
    // this is a system call. By making it static, it is run only once ( speed increase )
    // this gives us the absolute path, from the root, regardless where the server is hosted

    if (s == "/") {
        s = "/index.html";
    } // default

    if (!s.empty() && s[0] == '/') {
            s.remove_prefix(1);
    } // we remove the first /, if it exists, to prevent problems when appending the /

    std::filesystem::path target = root / s;

    if (!std::filesystem::exists(target))
    {
        std::cerr<<"DEBUG: FILE doesn't exist at path "<<target<<std::endl;
        return std::nullopt;
    }


    try{

        std::filesystem::path full_path = std::filesystem::canonical(target);
        // here we have the asbolute path of the destination file
        // canonical makes it so it is the same on all machines, also removes the /..
        // that can be used to access unwanted directories

        std::cerr << "[DEBUG] System tries to find file at: " << full_path << std::endl;

        auto [root_it, target_it] = std::mismatch(root.begin(), root.end(), full_path.begin(), full_path.end());
        // a filesystem object is like a vector of strings
        // the mismatch function returns the iterator of the first element
        // that is not the same in both objects

        //we check if that results is equal to root.end() to prevent any malicious users
        // and be extra safe

        //if this is true, the searched file is safely in the root path
        if (root_it == root.end() && std::filesystem::is_regular_file(full_path))
        {
            return full_path.string();
        }

    } catch(...){  // ... means catch all
        std::cerr << "[DEBUG] Error caught. System tried to find file at: " << target << std::endl;
        return std::nullopt;
    }
    
    std::cerr << "[DEBUG] System tried to find file at: " << target << std::endl;
    // in case the abs path is malicious/wrong/file doesn't exist
    return std::nullopt;
}

}