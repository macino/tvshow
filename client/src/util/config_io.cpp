#include "tvshow/util/config.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <sys/stat.h>

namespace tvshow::util {

Config load_config(std::string_view path) {
    const std::string p = path.empty() ? config_default_path() : std::string(path);
    std::ifstream in(p, std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream buf;
    buf << in.rdbuf();
    return parse_config(buf.str());
}

bool save_config(const Config& cfg, std::string_view path) {
    const std::string p = path.empty() ? config_default_path() : std::string(path);

    // Ensure parent directory exists (mkdir -p).
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
    out << "# tvshow configuration\n";
    out << "log-level = \"" << cfg.log_level << "\"\n";
    out << "address-bar = \"" << cfg.address_bar << "\"\n";
    out << "default-style = \"" << cfg.default_style << "\"\n";
    out << "image-renderer = \"" << cfg.image_renderer << "\"\n";
    if (!cfg.start_url.empty()) {
        out << "start-url = \"" << cfg.start_url << "\"\n";
    }
    if (!cfg.download_dir.empty()) {
        out << "download-dir = \"" << cfg.download_dir << "\"\n";
    }
    if (!cfg.handler_video.empty()) {
        out << "handler-video = \"" << cfg.handler_video << "\"\n";
    }
    if (!cfg.handler_audio.empty()) {
        out << "handler-audio = \"" << cfg.handler_audio << "\"\n";
    }
    for (const auto& [name, command] : cfg.window_providers) {
        out << "window-provider-" << name << " = \"" << command << "\"\n";
    }
    return out.good();
}

}  // namespace tvshow::util
