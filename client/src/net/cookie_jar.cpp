#include "tvshow/net/cookie_jar.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace tvshow::net {

namespace {

std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) { s.remove_prefix(1); }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) { s.remove_suffix(1); }
    return s;
}

// Case-insensitive ASCII comparison.
bool iequal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Parse one Set-Cookie header value, return a Cookie or nothing on parse error.
std::optional<Cookie> parse_set_cookie(std::string_view header, std::string_view request_host) {
    if (header.empty()) { return std::nullopt; }

    Cookie c;

    // First token: "name=value" (value may be empty, name must not be empty).
    const auto semi = header.find(';');
    const auto pair = trim(header.substr(0, semi));
    const auto eq = pair.find('=');
    if (eq == std::string_view::npos || eq == 0) { return std::nullopt; }
    c.name = std::string(trim(pair.substr(0, eq)));
    c.value = std::string(trim(pair.substr(eq + 1)));

    // Remaining tokens: cookie-av list.
    std::string_view rest = (semi == std::string_view::npos) ? "" : header.substr(semi + 1);
    c.domain = std::string(request_host);
    c.host_only = true;
    c.path = "/";

    while (!rest.empty()) {
        const auto next = rest.find(';');
        const auto av = trim(rest.substr(0, next));
        rest = (next == std::string_view::npos) ? "" : rest.substr(next + 1);

        if (av.empty()) { continue; }

        const auto av_eq = av.find('=');
        const auto attr_name = trim(av.substr(0, av_eq));
        const auto attr_val = (av_eq == std::string_view::npos)
                                  ? std::string_view{}
                                  : trim(av.substr(av_eq + 1));

        if (iequal(attr_name, "Domain")) {
            std::string_view dom = attr_val;
            if (dom.starts_with('.')) { dom.remove_prefix(1); }  // strip leading dot
            c.domain = std::string(dom);
            c.host_only = false;
        } else if (iequal(attr_name, "Path")) {
            c.path = attr_val.empty() ? "/" : std::string(attr_val);
        } else if (iequal(attr_name, "Max-Age")) {
            int seconds = 0;
            const auto res = std::from_chars(attr_val.data(), attr_val.data() + attr_val.size(), seconds);
            if (res.ec == std::errc{}) {
                c.expires_at = std::time(nullptr) + seconds;
            }
        } else if (iequal(attr_name, "Expires") && c.expires_at == 0) {
            // Max-Age takes precedence over Expires (RFC 6265 §5.3); only parse
            // Expires if Max-Age hasn't already set expires_at.
            std::tm tm{};
            std::string date_str(attr_val);
            if (::strptime(date_str.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) != nullptr) {
                c.expires_at = ::timegm(&tm);
            }
        }
        // Secure, HttpOnly, SameSite → still ignored in v1
    }

    // Domain must not be a public suffix or empty.
    if (c.domain.empty()) { return std::nullopt; }
    return c;
}

}  // namespace

void CookieJar::store(std::string_view host, const std::vector<std::string>& set_cookie_headers) {
    for (const auto& header : set_cookie_headers) {
        auto cookie = parse_set_cookie(header, host);
        if (!cookie) { continue; }
        upsert(std::move(*cookie));
    }
}

void CookieJar::upsert(Cookie cookie) {
    // Replace existing cookie with the same name+domain+path.
    auto it = std::find_if(cookies_.begin(), cookies_.end(), [&](const Cookie& existing) {
        return existing.name == cookie.name && existing.domain == cookie.domain &&
               existing.path == cookie.path;
    });
    if (it != cookies_.end()) {
        *it = std::move(cookie);
    } else {
        cookies_.push_back(std::move(cookie));
    }
}

bool CookieJar::matches(const Cookie& c, std::string_view host,
                         std::string_view path) const noexcept {
    // Domain check.
    if (c.host_only) {
        if (!iequal(c.domain, host)) { return false; }
    } else {
        // Subdomain match: host ends with ".domain" or equals domain.
        if (!iequal(c.domain, host)) {
            if (host.size() <= c.domain.size()) { return false; }
            const auto suffix_start = host.size() - c.domain.size();
            if (host[suffix_start - 1] != '.') { return false; }
            if (!iequal(host.substr(suffix_start), c.domain)) { return false; }
        }
    }

    // Path check: request path must start with the cookie path.
    if (!path.starts_with(c.path)) {
        // Special case: cookie path "/foo" matches "/foo/bar" but not "/foobar".
        // If path == cookie.path exactly, it matches.
        if (path != c.path) { return false; }
    }

    return true;
}

std::string CookieJar::cookie_header(std::string_view host, std::string_view path) const {
    std::string out;
    for (const auto& c : cookies_) {
        if (!matches(c, host, path)) { continue; }
        if (!out.empty()) { out += "; "; }
        out += c.name;
        out += '=';
        out += c.value;
    }
    return out;
}

namespace {

// Tab is never legal inside a cookie name/value/domain/path (RFC 6265 token
// rules), so a plain split on '\t' is safe — no escaping needed.
std::string_view next_field(std::string_view& line) {
    const auto tab = line.find('\t');
    const auto field = line.substr(0, tab);
    line = (tab == std::string_view::npos) ? std::string_view{} : line.substr(tab + 1);
    return field;
}

}  // namespace

std::string CookieJar::serialize_persistent() const {
    std::string out;
    for (const auto& c : cookies_) {
        if (c.expires_at == 0) { continue; }  // session cookie — never persisted
        out += c.domain;
        out += '\t';
        out += (c.host_only ? '1' : '0');
        out += '\t';
        out += c.path;
        out += '\t';
        out += c.name;
        out += '\t';
        out += c.value;
        out += '\t';
        out += std::to_string(static_cast<long long>(c.expires_at));
        out += '\n';
    }
    return out;
}

void CookieJar::load_persistent(std::string_view text, std::time_t now) {
    while (!text.empty()) {
        const auto nl = text.find('\n');
        auto line = text.substr(0, nl);
        text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);
        if (line.empty()) { continue; }

        Cookie c;
        c.domain = std::string(next_field(line));
        const auto host_only_field = next_field(line);
        c.host_only = host_only_field == "1";
        c.path = std::string(next_field(line));
        c.name = std::string(next_field(line));
        c.value = std::string(next_field(line));
        const auto expires_field = next_field(line);

        if (c.domain.empty() || c.name.empty()) { continue; }  // malformed line

        long long expires = 0;
        const auto res = std::from_chars(expires_field.data(),
                                          expires_field.data() + expires_field.size(), expires);
        if (res.ec != std::errc{} || expires == 0) { continue; }  // malformed / not persistent
        if (expires <= static_cast<long long>(now)) { continue; }  // expired — drop

        c.expires_at = static_cast<std::time_t>(expires);
        upsert(std::move(c));
    }
}

}  // namespace tvshow::net
