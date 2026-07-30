#include "tvshow/app/extension_ui_protocol.hpp"

#include <charconv>
#include <unordered_map>

namespace tvshow::app {

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

// Unescapes \" and \n inside a quoted value's raw contents.
std::string unescape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            const char next = raw[i + 1];
            if (next == '"') { out += '"'; ++i; continue; }
            if (next == 'n') { out += '\n'; ++i; continue; }
            if (next == '\\') { out += '\\'; ++i; continue; }
        }
        out += raw[i];
    }
    return out;
}

// Parses "key=value key2=value2 ..." into a map. Values are either a
// double-quoted string (with \" \n \\ escapes) or a bare unquoted token
// (used for integers). Malformed tokens are skipped, not fatal.
std::unordered_map<std::string, std::string> parse_fields(std::string_view rest) {
    std::unordered_map<std::string, std::string> fields;
    while (!rest.empty()) {
        rest = trim(rest);
        if (rest.empty()) { break; }

        const auto eq = rest.find('=');
        if (eq == std::string_view::npos) { break; }  // no more valid key=value tokens
        const std::string key(trim(rest.substr(0, eq)));
        rest = rest.substr(eq + 1);

        if (!rest.empty() && rest.front() == '"') {
            rest.remove_prefix(1);
            size_t i = 0;
            while (i < rest.size()) {
                if (rest[i] == '\\' && i + 1 < rest.size()) {
                    i += 2;
                    continue;
                }
                if (rest[i] == '"') { break; }
                ++i;
            }
            if (i >= rest.size()) { break; }  // unterminated quote -- malformed, stop
            fields[key] = unescape(rest.substr(0, i));
            rest = rest.substr(i + 1);
        } else {
            const auto sp = rest.find_first_of(" \t");
            fields[key] = std::string(rest.substr(0, sp));
            rest = (sp == std::string_view::npos) ? std::string_view{} : rest.substr(sp);
        }
    }
    return fields;
}

bool parse_int_field(const std::unordered_map<std::string, std::string>& fields,
                     std::string_view key, int& out) {
    const auto it = fields.find(std::string(key));
    if (it == fields.end()) { return false; }
    const auto res = std::from_chars(it->second.data(), it->second.data() + it->second.size(), out);
    return res.ec == std::errc{};
}

}  // namespace

UiCommand parse_ui_command(std::string_view line) {
    line = trim(line);
    if (line.empty()) { return UiIgnored{}; }

    const auto sp = line.find_first_of(" \t");
    const std::string_view word = line.substr(0, sp);
    const std::string_view rest = (sp == std::string_view::npos) ? std::string_view{}
                                                                  : line.substr(sp + 1);

    if (word == "UI_INIT") { return UiInit{}; }
    if (word == "CLEAR") { return UiClear{}; }

    const auto fields = parse_fields(rest);

    if (word == "BUTTON") {
        UiButton b;
        const auto label_it = fields.find("label");
        if (!parse_int_field(fields, "id", b.id) || !parse_int_field(fields, "x", b.x) ||
            !parse_int_field(fields, "y", b.y) || !parse_int_field(fields, "w", b.w) ||
            label_it == fields.end()) {
            return UiIgnored{};
        }
        b.label = label_it->second;
        return b;
    }

    if (word == "TEXT") {
        UiText t;
        const auto value_it = fields.find("value");
        if (!parse_int_field(fields, "id", t.id) || !parse_int_field(fields, "x", t.x) ||
            !parse_int_field(fields, "y", t.y) || !parse_int_field(fields, "w", t.w) ||
            value_it == fields.end()) {
            return UiIgnored{};
        }
        if (!parse_int_field(fields, "h", t.h)) { t.h = 1; }
        t.value = value_it->second;
        return t;
    }

    if (word == "TITLE") {
        const auto value_it = fields.find("value");
        if (value_it == fields.end()) { return UiIgnored{}; }
        return UiTitle{value_it->second};
    }

    return UiIgnored{};
}

std::string format_click_event(int id) {
    return "CLICK id=" + std::to_string(id);
}

}  // namespace tvshow::app
