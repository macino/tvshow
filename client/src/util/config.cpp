#include "tvshow/util/config.hpp"

#include <string>
#include <string_view>

namespace tvshow::util {

namespace {

std::string_view trim_sv(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

Config parse_config(std::string_view toml) {
    Config cfg;
    while (!toml.empty()) {
        const auto nl = toml.find('\n');
        const auto line = trim_sv(toml.substr(0, nl));
        toml = (nl == std::string_view::npos) ? "" : toml.substr(nl + 1);

        if (line.empty() || line.front() == '#') { continue; }

        const auto eq = line.find('=');
        if (eq == std::string_view::npos) { continue; }

        const auto key = trim_sv(line.substr(0, eq));
        const auto raw = trim_sv(line.substr(eq + 1));

        std::string val;
        if (!raw.empty() && raw.front() == '"') {
            // Quoted string: content between first and next closing '"'.
            const auto close = raw.find('"', 1);
            if (close != std::string_view::npos) {
                val = std::string(raw.substr(1, close - 1));
            }
        } else {
            // Bare value: strip trailing inline comment (# and everything after).
            auto bare = raw;
            const auto hash = bare.find('#');
            if (hash != std::string_view::npos) { bare = trim_sv(bare.substr(0, hash)); }
            val = std::string(bare);
        }

        if (key == "log-level")        { cfg.log_level     = std::move(val); }
        else if (key == "address-bar") { cfg.address_bar   = std::move(val); }
        else if (key == "start-url")   { cfg.start_url     = std::move(val); }
        else if (key == "default-style") { cfg.default_style = std::move(val); }
        else if (key == "image-renderer") { cfg.image_renderer = std::move(val); }
        else if (key == "download-dir") { cfg.download_dir = std::move(val); }
        else if (key == "handler-video") { cfg.handler_video = std::move(val); }
        else if (key == "handler-audio") { cfg.handler_audio = std::move(val); }
        else if (key.starts_with("window-provider-")) {
            cfg.window_providers.emplace_back(std::string(key.substr(16)), std::move(val));
        }
        else if (key == "translator-script") { cfg.translator_script = std::move(val); }
        else if (key == "extension-server-port") {
            int parsed = 0;
            bool valid = !val.empty();
            for (const char c : val) {
                if (c < '0' || c > '9') { valid = false; break; }
                parsed = parsed * 10 + (c - '0');
            }
            if (valid) { cfg.extension_server_port = parsed; }
        }
        // Unknown keys are silently ignored.
    }
    return cfg;
}

std::string config_default_path() {
    const char* xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/tvshow/config.toml";
    }
    const char* home = ::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/tvshow/config.toml";
}

}  // namespace tvshow::util
