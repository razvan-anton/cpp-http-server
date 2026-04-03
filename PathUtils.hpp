#include <string>
#include <filesystem>
#include <optional> // for std::optinal<string
#include <algorithm> //for mismatch

namespace File{

    std::optional<std::string> get_abs_path(const std::string &s);

}