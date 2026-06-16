#include "tvshow/util/url.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace tvshow::util {

namespace {

bool is_unreserved(char c) {
    const auto uc = static_cast<unsigned char>(c);
    return (uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9') ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

// RFC 3986 §5.2.4 — remove dot segments from a path.
std::string remove_dot_segments(std::string input) {
    std::string out;
    out.reserve(input.size());

    while (!input.empty()) {
        if (input.starts_with("../")) {
            input.erase(0, 3);
        } else if (input.starts_with("./")) {
            input.erase(0, 2);
        } else if (input.starts_with("/./")) {
            input.replace(0, 3, "/");
        } else if (input == "/.") {
            input = "/";
        } else if (input.starts_with("/../")) {
            input.replace(0, 4, "/");
            const auto pos = out.rfind('/');
            if (pos != std::string::npos)
                out.erase(pos);
        } else if (input == "/..") {
            input = "/";
            const auto pos = out.rfind('/');
            if (pos != std::string::npos)
                out.erase(pos);
        } else if (input == "." || input == "..") {
            input.clear();
        } else {
            const size_t start = (input.front() == '/') ? 1U : 0U;
            const auto seg_end = input.find('/', start);
            if (seg_end == std::string::npos) {
                out += input;
                input.clear();
            } else {
                out += input.substr(0, seg_end);
                input.erase(0, seg_end);
            }
        }
    }
    return out;
}

struct ParsedRef {
    std::string path;
    std::string query;
    std::string fragment;
};

ParsedRef split_path_query_fragment(std::string_view s) {
    ParsedRef r;
    const auto qpos = s.find('?');
    const auto fpos = s.find('#');

    if (qpos != std::string_view::npos && (fpos == std::string_view::npos || qpos < fpos)) {
        r.path = std::string(s.substr(0, qpos));
        if (fpos != std::string_view::npos) {
            r.query = std::string(s.substr(qpos + 1, fpos - qpos - 1));
            r.fragment = std::string(s.substr(fpos + 1));
        } else {
            r.query = std::string(s.substr(qpos + 1));
        }
    } else if (fpos != std::string_view::npos) {
        r.path = std::string(s.substr(0, fpos));
        r.fragment = std::string(s.substr(fpos + 1));
    } else {
        r.path = std::string(s);
    }
    return r;
}

}  // namespace

std::optional<Url> Url::parse(std::string_view s) {
    Url url;

    const auto scheme_end = s.find("://");
    if (scheme_end == std::string_view::npos)
        return std::nullopt;

    url.scheme_ = std::string(s.substr(0, scheme_end));
    if (url.scheme_ != "http" && url.scheme_ != "https")
        return std::nullopt;
    s.remove_prefix(scheme_end + 3);

    const auto auth_end = s.find_first_of("/?#");
    std::string_view authority;
    if (auth_end == std::string_view::npos) {
        authority = s;
        s = {};
    } else {
        authority = s.substr(0, auth_end);
        s.remove_prefix(auth_end);
    }

    // Parse host[:port] from authority (no userinfo in v1).
    const auto colon = authority.rfind(':');
    if (colon != std::string_view::npos) {
        uint16_t port = 0;
        const auto port_sv = authority.substr(colon + 1);
        auto [ptr, ec] = std::from_chars(port_sv.data(), port_sv.data() + port_sv.size(), port);
        if (ec == std::errc{} && ptr == port_sv.data() + port_sv.size()) {
            url.host_ = std::string(authority.substr(0, colon));
            url.port_ = port;
        } else {
            url.host_ = std::string(authority);
        }
    } else {
        url.host_ = std::string(authority);
    }

    if (url.host_.empty())
        return std::nullopt;

    if (!s.empty()) {
        auto pqf = split_path_query_fragment(s);
        url.path_ = pqf.path.empty() ? "/" : pqf.path;
        url.query_ = pqf.query;
        url.fragment_ = pqf.fragment;
    } else {
        url.path_ = "/";
    }

    return url;
}

std::optional<Url> Url::resolve(std::string_view ref) const {
    if (ref.empty())
        return *this;

    if (auto abs = Url::parse(ref); abs.has_value())
        return abs;
    if (ref.starts_with("//"))
        return std::nullopt;

    Url result = *this;
    result.fragment_.clear();
    result.query_.clear();

    if (ref.starts_with('#')) {
        result.path_ = path_;
        result.query_ = query_;
        result.fragment_ = std::string(ref.substr(1));
        return result;
    }

    if (ref.starts_with('?')) {
        result.path_ = path_;
        const auto fpos = ref.find('#');
        if (fpos != std::string_view::npos) {
            result.query_ = std::string(ref.substr(1, fpos - 1));
            result.fragment_ = std::string(ref.substr(fpos + 1));
        } else {
            result.query_ = std::string(ref.substr(1));
        }
        return result;
    }

    auto pqf = split_path_query_fragment(ref);
    result.query_ = pqf.query;
    result.fragment_ = pqf.fragment;

    if (ref.starts_with('/')) {
        result.path_ = remove_dot_segments(pqf.path);
    } else {
        std::string base = path_;
        const auto slash = base.rfind('/');
        if (slash != std::string::npos) {
            base.erase(slash + 1);
        } else {
            base = "/";
        }
        result.path_ = remove_dot_segments(base + pqf.path);
    }

    return result;
}

std::string Url::to_string() const {
    std::string s = scheme_ + "://" + host_;
    if (port_ != 0)
        s += ':' + std::to_string(port_);
    s += path_.empty() ? "/" : path_;
    if (!query_.empty())
        s += '?' + query_;
    if (!fragment_.empty())
        s += '#' + fragment_;
    return s;
}

uint16_t Url::effective_port() const noexcept {
    if (port_ != 0)
        return port_;
    return scheme_ == "https" ? static_cast<uint16_t>(443U) : static_cast<uint16_t>(80U);
}

std::string percent_decode(std::string_view s) {
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        if (s.at(i) == '%' && i + 2 < s.size()) {
            uint8_t val = 0;
            auto [ptr, ec] = std::from_chars(s.data() + i + 1, s.data() + i + 3, val, 16);
            if (ec == std::errc{} && ptr == s.data() + i + 3) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += s.at(i);
            }
        } else if (s.at(i) == '+') {
            result += ' ';
        } else {
            result += s.at(i);
        }
    }
    return result;
}

std::string percent_encode(std::string_view s) {
    static constexpr std::string_view kHex = "0123456789ABCDEF";
    std::string result;
    result.reserve(s.size() * 3U);

    for (const char ch : s) {
        if (is_unreserved(ch)) {
            result += ch;
        } else {
            const auto uc = static_cast<uint8_t>(ch);
            result += '%';
            result += kHex.at(uc >> 4U);
            result += kHex.at(uc & 0x0FU);
        }
    }
    return result;
}

std::string resolve_file_url(std::string_view base, std::string_view href) {
    constexpr std::string_view k_prefix = "file://";
    if (href.empty()) {
        return std::string(base);
    }
    if (href.starts_with(k_prefix)) {
        return std::string(href);
    }
    const std::string_view base_path =
        base.starts_with(k_prefix) ? base.substr(k_prefix.size()) : base;
    if (href.starts_with('/')) {
        return std::string(k_prefix) + remove_dot_segments(std::string(href));
    }
    const auto slash = base_path.rfind('/');
    const std::string_view dir =
        slash == std::string_view::npos ? std::string_view{} : base_path.substr(0, slash + 1);
    return std::string(k_prefix) + remove_dot_segments(std::string(dir) + std::string(href));
}

}  // namespace tvshow::util
