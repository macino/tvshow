#include "tvshow/net/cookie_jar.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using tvshow::net::CookieJar;

// ── store / parse Set-Cookie ─────────────────────────────────────────────────

TEST_CASE("CookieJar: stores and retrieves a simple host-only cookie") {
    CookieJar jar;
    jar.store("example.com", {"session=abc123"});
    const std::string h = jar.cookie_header("example.com", "/");
    CHECK(h == "session=abc123");
}

TEST_CASE("CookieJar: host-only cookie does not match subdomain") {
    CookieJar jar;
    jar.store("example.com", {"id=42"});
    CHECK(jar.cookie_header("sub.example.com", "/").empty());
}

TEST_CASE("CookieJar: Domain attribute enables subdomain matching") {
    CookieJar jar;
    jar.store("example.com", {"token=xyz; Domain=example.com"});
    CHECK(jar.cookie_header("sub.example.com", "/") == "token=xyz");
    CHECK(jar.cookie_header("example.com", "/") == "token=xyz");
}

TEST_CASE("CookieJar: Domain attribute with leading dot is stripped") {
    CookieJar jar;
    jar.store("example.com", {"k=v; Domain=.example.com"});
    CHECK(jar.cookie_header("api.example.com", "/") == "k=v");
}

TEST_CASE("CookieJar: Path attribute restricts matching") {
    CookieJar jar;
    jar.store("example.com", {"pref=1; Path=/admin"});
    CHECK(jar.cookie_header("example.com", "/admin/settings").empty() == false);
    CHECK(jar.cookie_header("example.com", "/").empty());
}

TEST_CASE("CookieJar: multiple cookies in header value") {
    CookieJar jar;
    jar.store("example.com", {"a=1", "b=2"});
    const std::string h = jar.cookie_header("example.com", "/");
    // order is insertion order; both must appear
    CHECK(h.find("a=1") != std::string::npos);
    CHECK(h.find("b=2") != std::string::npos);
    CHECK(h.find("; ") != std::string::npos);
}

TEST_CASE("CookieJar: later Set-Cookie replaces earlier same name") {
    CookieJar jar;
    jar.store("example.com", {"tok=old"});
    jar.store("example.com", {"tok=new"});
    CHECK(jar.cookie_header("example.com", "/") == "tok=new");
}

TEST_CASE("CookieJar: returns empty when no cookies match host") {
    CookieJar jar;
    jar.store("example.com", {"x=1"});
    CHECK(jar.cookie_header("other.com", "/").empty());
}

TEST_CASE("CookieJar: clear removes all cookies") {
    CookieJar jar;
    jar.store("example.com", {"a=1"});
    jar.clear();
    CHECK(jar.cookie_header("example.com", "/").empty());
    CHECK(jar.empty());
}

TEST_CASE("CookieJar: ignores malformed Set-Cookie (no equals sign)") {
    CookieJar jar;
    jar.store("example.com", {"badcookie"});
    CHECK(jar.empty());
}

// ── persistence (q-persisted-cookies) ───────────────────────────────────────

TEST_CASE("CookieJar: Max-Age sets a persistent cookie, serialized") {
    CookieJar jar;
    jar.store("example.com", {"tok=abc; Max-Age=3600"});
    const auto out = jar.serialize_persistent();
    CHECK(out.find("example.com") != std::string::npos);
    CHECK(out.find("tok") != std::string::npos);
    CHECK(out.find("abc") != std::string::npos);
}

TEST_CASE("CookieJar: session cookie (no Max-Age/Expires) is never serialized") {
    CookieJar jar;
    jar.store("example.com", {"session=xyz"});
    CHECK(jar.serialize_persistent().empty());
}

TEST_CASE("CookieJar: Max-Age takes precedence over Expires") {
    CookieJar jar;
    jar.store("example.com",
              {"tok=abc; Max-Age=60; Expires=Wed, 01 Jan 2020 00:00:00 GMT"});
    // Expires (2020) would already be in the past; Max-Age=60 wins -> not expired.
    const std::time_t now = std::time(nullptr);
    CookieJar reloaded;
    reloaded.load_persistent(jar.serialize_persistent(), now);
    CHECK(reloaded.cookie_header("example.com", "/") == "tok=abc");
}

TEST_CASE("CookieJar: round-trips a persistent cookie through serialize/load") {
    CookieJar jar;
    jar.store("example.com", {"pref=dark; Max-Age=3600; Path=/settings"});

    CookieJar reloaded;
    reloaded.load_persistent(jar.serialize_persistent());
    CHECK(reloaded.cookie_header("example.com", "/settings") == "pref=dark");
}

TEST_CASE("CookieJar: load_persistent drops already-expired entries") {
    CookieJar jar;
    // expires_at = 100 (epoch), now = 1000 -> expired, must not load.
    jar.load_persistent("example.com\t1\t/\ttok\tval\t100", /*now=*/1000);
    CHECK(jar.empty());
}

TEST_CASE("CookieJar: load_persistent keeps not-yet-expired entries") {
    CookieJar jar;
    jar.load_persistent("example.com\t1\t/\ttok\tval\t2000", /*now=*/1000);
    CHECK(jar.cookie_header("example.com", "/") == "tok=val");
}

TEST_CASE("CookieJar: load_persistent ignores malformed lines") {
    CookieJar jar;
    jar.load_persistent("not-enough-fields", /*now=*/1000);
    CHECK(jar.empty());
}

TEST_CASE("CookieJar: serialize_persistent round-trips host_only flag") {
    CookieJar jar;
    jar.store("example.com", {"a=1; Domain=example.com; Max-Age=3600"});
    CookieJar reloaded;
    reloaded.load_persistent(jar.serialize_persistent());
    CHECK(reloaded.cookie_header("sub.example.com", "/") == "a=1");
}
