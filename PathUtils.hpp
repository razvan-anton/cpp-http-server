#include <string>
#include <string_view>
#include <filesystem>
#include <optional> // for std::optional<string>
#include <algorithm> //for mismatch
#include <iostream> //for debugging
#include <cstddef> // for size_t
#include <sys/stat.h>   // for fstat

namespace File{

    std::optional<std::string> get_abs_path(std::string_view &s);

    size_t get_file_size(const int file_fd);

}