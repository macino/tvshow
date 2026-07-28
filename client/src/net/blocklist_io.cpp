#include "tvshow/net/blocklist.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace tvshow::net {

std::string blocklist_default_path() {
    const char* xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/tvshow/blocklist";
    }
    const char* home = ::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/tvshow/blocklist";
}

Blocklist load_blocklist(std::string_view path) {
    const std::string p = path.empty() ? blocklist_default_path() : std::string(path);
    std::ifstream in(p, std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream buf;
    buf << in.rdbuf();
    return parse_blocklist(buf.str());
}

}  // namespace tvshow::net
