#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tvshow::net {

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;   // host-only if the Set-Cookie had no Domain attr
    bool host_only = true;  // true → exact match only; false → subdomain match
    std::string path;     // "/" if the Set-Cookie had no Path attr
};

// In-memory per-session cookie store. Not thread-safe; loads are sequential
// (each background thread is joined before the next starts) so no locking is
// required.  Parse and inject cookies per RFC 6265 §5 (simplified: no
// SameSite, no Secure/HttpOnly gating, no expiry — session cookies only).
class CookieJar {
public:
    CookieJar() = default;

    // Parse `set_cookie_headers` (all Set-Cookie values for a response from
    // `host`) and add matching cookies to the jar.
    void store(std::string_view host, const std::vector<std::string>& set_cookie_headers);

    // Return a "Cookie:" header value for a request to `host`/`path`.
    // Returns "" when no cookies match (caller should omit the header).
    [[nodiscard]] std::string cookie_header(std::string_view host,
                                            std::string_view path) const;

    [[nodiscard]] bool empty() const noexcept { return cookies_.empty(); }
    void clear() noexcept { cookies_.clear(); }

private:
    std::vector<Cookie> cookies_;

    [[nodiscard]] bool matches(const Cookie& c, std::string_view host,
                               std::string_view path) const noexcept;
};

}  // namespace tvshow::net
