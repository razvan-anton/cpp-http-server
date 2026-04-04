#include <string>
#include <string_view>
#include <filesystem>
#include <optional> // for std::optional<string>
#include <algorithm> //for mismatch
#include <iostream> //for debugging

namespace File{

    std::optional<std::string> get_abs_path(std::string_view &s);

}