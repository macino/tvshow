#include "tvshow/net/extension_manifest.hpp"

namespace tvshow::net {

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

std::optional<ExtensionManifest> parse_extension_manifest(std::string_view toml) {
    ExtensionManifest m;
    while (!toml.empty()) {
        const auto nl = toml.find('\n');
        const auto line = trim_sv(toml.substr(0, nl));
        toml = (nl == std::string_view::npos) ? "" : toml.substr(nl + 1);

        if (line.empty() || line.front() == '#') { continue; }
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) { continue; }

        const auto key = trim_sv(line.substr(0, eq));
        auto raw = trim_sv(line.substr(eq + 1));
        std::string val;
        if (!raw.empty() && raw.front() == '"') {
            const auto close = raw.find('"', 1);
            if (close != std::string_view::npos) { val = std::string(raw.substr(1, close - 1)); }
        } else {
            val = std::string(raw);
        }

        if (key == "name") { m.name = std::move(val); }
        else if (key == "entry") { m.entry = std::move(val); }
        else if (key == "install") { m.install = std::move(val); }
    }
    if (m.name.empty() || m.entry.empty()) { return std::nullopt; }
    return m;
}

CgiResponse parse_cgi_response(std::string_view raw) {
    CgiResponse resp;
    std::string_view rest = raw;
    // Consume header lines up to (and including) the first blank line --
    // whatever's left after that is the body, verbatim.
    while (true) {
        const auto nl = rest.find('\n');
        const std::string_view line = (nl == std::string_view::npos) ? rest : rest.substr(0, nl);
        if (line.empty()) {
            rest = (nl == std::string_view::npos) ? "" : rest.substr(nl + 1);
            break;
        }
        if (nl == std::string_view::npos) {
            // No blank-line separator at all -- everything was headers.
            rest = "";
            const auto colon = line.find(':');
            if (colon != std::string_view::npos) {
                const auto key = trim_sv(line.substr(0, colon));
                const auto value = trim_sv(line.substr(colon + 1));
                resp.headers.emplace_back(std::string(key), std::string(value));
            }
            break;
        }

        const auto colon = line.find(':');
        if (colon != std::string_view::npos) {
            const auto key = trim_sv(line.substr(0, colon));
            const auto value = trim_sv(line.substr(colon + 1));
            if (key == "Status") {
                int parsed = 0;
                for (const char c : value) {
                    if (c < '0' || c > '9') { parsed = 0; break; }
                    parsed = parsed * 10 + (c - '0');
                }
                if (parsed > 0) { resp.status = parsed; }
            } else {
                resp.headers.emplace_back(std::string(key), std::string(value));
            }
        }
        rest = rest.substr(nl + 1);
    }
    resp.body = std::string(rest);
    return resp;
}

std::string build_cgi_request(std::string_view method, std::string_view path,
                              std::string_view query, std::string_view body) {
    std::string out;
    out += "METHOD ";
    out += method;
    out += "\nPATH ";
    out += path;
    out += "\nQUERY ";
    out += query;
    out += "\n\n";
    out += body;
    return out;
}

}  // namespace tvshow::net
