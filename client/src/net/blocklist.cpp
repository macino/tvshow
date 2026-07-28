#include "tvshow/net/blocklist.hpp"

namespace tvshow::net {

namespace {

std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

Blocklist parse_blocklist(std::string_view text) {
    Blocklist list;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        const auto line = trim(text.substr(0, nl));
        text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);

        if (line.empty() || line.front() == '#') { continue; }

        if (line.starts_with("block:")) {
            const auto pattern = trim(line.substr(6));
            if (!pattern.empty()) { list.block_globs.emplace_back(pattern); }
        } else if (line.starts_with("hide:")) {
            const auto selector = trim(line.substr(5));
            if (!selector.empty()) { list.hide_selectors.emplace_back(selector); }
        }
        // Unknown prefix -> ignored (forward-compat, same stance as Config).
    }
    return list;
}

bool glob_match(std::string_view pattern, std::string_view text) noexcept {
    // Classic two-pointer glob match (iterative, no recursion/backtracking blowup).
    std::size_t p = 0, t = 0;
    std::size_t star_p = std::string_view::npos, star_t = 0;

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star_p = p;
            star_t = t;
            ++p;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            ++star_t;
            t = star_t;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') { ++p; }
    return p == pattern.size();
}

bool is_blocked(const Blocklist& list, std::string_view url) noexcept {
    for (const auto& glob : list.block_globs) {
        if (glob_match(glob, url)) { return true; }
    }
    return false;
}

}  // namespace tvshow::net
