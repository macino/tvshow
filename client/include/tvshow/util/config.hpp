#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::util {

// Recognised settings. Unrecognised keys are silently ignored (forward compat).
struct Config {
    std::string log_level     = "warn";   // debug / info / warn / error
    std::string address_bar   = "modal";  // modal | persistent
    std::string start_url;                // empty = no initial navigation
    std::string default_style = "auto";   // auto | tvision | light | dark
    std::string image_renderer = "alt";   // alt | braille
    std::string download_dir;             // last-used "Save Link As" directory (q-download-manager)
    // adr-external-handlers: fire-and-forget spawn on Enter over a matching
    // media link. "%s" in the template is replaced with the absolute URL.
    // Empty = no handler configured, falls back to normal link navigation.
    std::string handler_video;  // handler-video, e.g. "mpv %s"
    std::string handler_audio;  // handler-audio
    // adr-external-window-provider: `window-provider-<name> = "<command>"`
    // entries, name -> shell-word-split command (no %s substitution -- input
    // goes over stdin once the window is open, not as an argv URL).
    std::vector<std::pair<std::string, std::string>> window_providers;
    // adr-translator-native-window: absolute path to translator.py. Empty =
    // Translate always shows a config-missing error, never spawns anything.
    std::string translator_script;
    // adr-extension-server: port for the internal per-request HTTP gateway
    // that routes /extensions/<name>/ to a bundled or installed extension.
    // Server starts lazily on first use, not at every launch.
    int extension_server_port = 8765;
};

// Parse TOML content (pure — no I/O). Only supports the subset:
//   key = "string"   key = bare_word
// Lines starting with '#' and blank lines are ignored.
[[nodiscard]] Config parse_config(std::string_view toml);

// Path to the user's config file: ${XDG_CONFIG_HOME:-~/.config}/tvshow/config.toml
[[nodiscard]] std::string config_default_path();

// Read and parse the config file at `path` (or the default path when empty).
// Returns a default Config on any read/parse failure (silent — no log).
[[nodiscard]] Config load_config(std::string_view path = "");

// Write cfg to `path` (or the default path when empty). Creates parent dirs.
// Returns false on write failure.
bool save_config(const Config& cfg, std::string_view path = "");

}  // namespace tvshow::util
