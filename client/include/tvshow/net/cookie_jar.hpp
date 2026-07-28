#pragma once

#include <ctime>
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
    // 0 = session cookie (no Max-Age/Expires) — dropped on exit, never persisted.
    // >0 = epoch seconds the cookie expires at (persistent, written to disk).
    std::time_t expires_at = 0;
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

    // Persistent cookies only (expires_at != 0), one per line, tab-separated:
    // domain\thost_only(0|1)\tpath\tname\tvalue\texpires_at
    [[nodiscard]] std::string serialize_persistent() const;

    // Parses lines in the format written by serialize_persistent(). Cookies
    // whose expires_at <= now are dropped (not added). Existing cookies with
    // the same name+domain+path are replaced, same rule as store().
    void load_persistent(std::string_view text, std::time_t now = std::time(nullptr));

    [[nodiscard]] const std::vector<Cookie>& cookies() const noexcept { return cookies_; }

private:
    std::vector<Cookie> cookies_;

    [[nodiscard]] bool matches(const Cookie& c, std::string_view host,
                               std::string_view path) const noexcept;

    void upsert(Cookie cookie);
};

// Reads/writes the persistent-cookie file at `path` (or the default
// XDG_CONFIG_HOME-respecting location when empty). I/O only — parsing lives
// in CookieJar::serialize_persistent/load_persistent above.
[[nodiscard]] std::string cookie_jar_default_path();
void load_cookie_jar(CookieJar& jar, std::string_view path = "");
bool save_cookie_jar(const CookieJar& jar, std::string_view path = "");

}  // namespace tvshow::net
