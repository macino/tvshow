#include "tvshow/net/cookie_jar.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <sys/stat.h>

namespace tvshow::net {

std::string cookie_jar_default_path() {
    const char* xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/tvshow/cookies";
    }
    const char* home = ::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/tvshow/cookies";
}

void load_cookie_jar(CookieJar& jar, std::string_view path) {
    const std::string p = path.empty() ? cookie_jar_default_path() : std::string(path);
    std::ifstream in(p, std::ios::binary);
    if (!in) { return; }
    std::ostringstream buf;
    buf << in.rdbuf();
    jar.load_persistent(buf.str());
}

bool save_cookie_jar(const CookieJar& jar, std::string_view path) {
    const std::string p = path.empty() ? cookie_jar_default_path() : std::string(path);

    // Ensure parent directory exists (mkdir -p), same pattern as save_config.
    const auto last_slash = p.rfind('/');
    if (last_slash != std::string::npos) {
        const std::string dir = p.substr(0, last_slash);
        for (std::size_t i = 1; i < dir.size(); ++i) {
            if (dir[i] == '/') { ::mkdir(dir.substr(0, i).c_str(), S_IRWXU); }
        }
        ::mkdir(dir.c_str(), S_IRWXU);
    }

    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) { return false; }
    out << jar.serialize_persistent();
    return out.good();
}

}  // namespace tvshow::net
