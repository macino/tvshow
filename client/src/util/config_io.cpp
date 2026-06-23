#include "tvshow/util/config.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace tvshow::util {

Config load_config(std::string_view path) {
    const std::string p = path.empty() ? config_default_path() : std::string(path);
    std::ifstream in(p, std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream buf;
    buf << in.rdbuf();
    return parse_config(buf.str());
}

}  // namespace tvshow::util
