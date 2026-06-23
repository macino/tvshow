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
