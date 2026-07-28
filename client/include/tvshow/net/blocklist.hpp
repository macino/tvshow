#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tvshow::net {

// Content/ad blocklist rules (adr-content-blocklist). Two rule kinds:
//   block: <url-glob>   — matched request never goes out, synthetic empty page instead
//   hide: <css-selector> — implicit lowest-author-priority `display: none`
struct Blocklist {
    std::vector<std::string> block_globs;
    std::vector<std::string> hide_selectors;

    [[nodiscard]] bool empty() const noexcept {
        return block_globs.empty() && hide_selectors.empty();
    }
};

// Parses "block: <glob>" / "hide: <selector>" lines (one per line). Blank
// lines and lines starting with '#' are ignored. Malformed lines (unknown
// prefix, empty pattern) are silently skipped — same forward-compat stance
// as Config's unknown-key handling.
[[nodiscard]] Blocklist parse_blocklist(std::string_view text);

// Shell-glob-style match: '*' matches any run of characters (incl. none),
// '?' matches exactly one character. No character classes, no escaping.
// Case-sensitive (URL paths/queries are case-sensitive).
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view text) noexcept;

// True if `url` matches any rule in block_globs.
[[nodiscard]] bool is_blocked(const Blocklist& list, std::string_view url) noexcept;

// Path to the user's blocklist file: ${XDG_CONFIG_HOME:-~/.config}/tvshow/blocklist
[[nodiscard]] std::string blocklist_default_path();

// Read and parse the blocklist file at `path` (or the default path when
// empty). Returns an empty Blocklist on any read failure (silent, same
// degrade-gracefully stance as load_config).
[[nodiscard]] Blocklist load_blocklist(std::string_view path = "");

}  // namespace tvshow::net
