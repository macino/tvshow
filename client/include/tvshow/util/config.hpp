#pragma once

#include <string>
#include <string_view>

namespace tvshow::util {

// Recognised settings. Unrecognised keys are silently ignored (forward compat).
struct Config {
    std::string log_level = "warn";      // debug / info / warn / error
    std::string address_bar = "modal";   // modal | persistent
    std::string start_url;               // empty = no initial navigation
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

}  // namespace tvshow::util
