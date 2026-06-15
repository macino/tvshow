#include "tvshow/util/url.hpp"

#include <doctest/doctest.h>

using tvshow::util::percent_decode;
using tvshow::util::percent_encode;
using tvshow::util::Url;

TEST_CASE("Url::parse: basic HTTP URL") {
    auto u = Url::parse("http://example.com/path?q=1#frag");
    REQUIRE(u.has_value());
    CHECK(u->scheme() == "http");
    CHECK(u->host() == "example.com");
    CHECK(u->port() == 0);
    CHECK(u->effective_port() == 80);
    CHECK(u->path() == "/path");
    CHECK(u->query() == "q=1");
    CHECK(u->fragment() == "frag");
}

TEST_CASE("Url::parse: explicit port") {
    auto u = Url::parse("http://localhost:8080/");
    REQUIRE(u.has_value());
    CHECK(u->host() == "localhost");
    CHECK(u->port() == 8080);
    CHECK(u->effective_port() == 8080);
    CHECK(u->path() == "/");
}

TEST_CASE("Url::parse: HTTPS default port") {
    auto u = Url::parse("https://example.com/");
    REQUIRE(u.has_value());
    CHECK(u->scheme() == "https");
    CHECK(u->port() == 0);
    CHECK(u->effective_port() == 443);
}

TEST_CASE("Url::parse: no path defaults to /") {
    auto u = Url::parse("http://example.com");
    REQUIRE(u.has_value());
    CHECK(u->path() == "/");
    CHECK(u->query().empty());
    CHECK(u->fragment().empty());
}

TEST_CASE("Url::parse: query only (no fragment)") {
    auto u = Url::parse("http://example.com/search?q=hello+world");
    REQUIRE(u.has_value());
    CHECK(u->path() == "/search");
    CHECK(u->query() == "q=hello+world");
    CHECK(u->fragment().empty());
}

TEST_CASE("Url::parse: rejects non-http schemes") {
    CHECK_FALSE(Url::parse("ftp://example.com/").has_value());
    CHECK_FALSE(Url::parse("file:///etc/passwd").has_value());
    CHECK_FALSE(Url::parse("ws://example.com/").has_value());
}

TEST_CASE("Url::parse: rejects no scheme") {
    CHECK_FALSE(Url::parse("/just/a/path").has_value());
    CHECK_FALSE(Url::parse("relative/path").has_value());
    CHECK_FALSE(Url::parse("").has_value());
}

TEST_CASE("Url::to_string round-trip") {
    const std::string raw = "http://example.com:9000/a/b?x=1#top";
    auto u = Url::parse(raw);
    REQUIRE(u.has_value());
    CHECK(u->to_string() == raw);
}

TEST_CASE("Url::to_string: no port omits it") {
    auto u = Url::parse("http://example.com/");
    REQUIRE(u.has_value());
    CHECK(u->to_string() == "http://example.com/");
}

TEST_CASE("Url::resolve: absolute reference replaces entire URL") {
    auto base = Url::parse("http://example.com/a/b");
    REQUIRE(base.has_value());
    auto r = base->resolve("http://other.com/x");
    REQUIRE(r.has_value());
    CHECK(r->host() == "other.com");
    CHECK(r->path() == "/x");
}

TEST_CASE("Url::resolve: absolute path replaces path") {
    auto base = Url::parse("http://example.com/a/b?q=1");
    REQUIRE(base.has_value());
    auto r = base->resolve("/new/path");
    REQUIRE(r.has_value());
    CHECK(r->host() == "example.com");
    CHECK(r->path() == "/new/path");
    CHECK(r->query().empty());
}

TEST_CASE("Url::resolve: relative path merges with base") {
    auto base = Url::parse("http://example.com/a/b/c");
    REQUIRE(base.has_value());
    auto r = base->resolve("../d");
    REQUIRE(r.has_value());
    CHECK(r->path() == "/a/d");
}

TEST_CASE("Url::resolve: same-dir relative") {
    auto base = Url::parse("http://example.com/a/b");
    REQUIRE(base.has_value());
    auto r = base->resolve("c");
    REQUIRE(r.has_value());
    CHECK(r->path() == "/a/c");
}

TEST_CASE("Url::resolve: fragment-only reference") {
    auto base = Url::parse("http://example.com/page?q=1");
    REQUIRE(base.has_value());
    auto r = base->resolve("#section");
    REQUIRE(r.has_value());
    CHECK(r->path() == "/page");
    CHECK(r->query() == "q=1");
    CHECK(r->fragment() == "section");
}

TEST_CASE("Url::resolve: query-only reference") {
    auto base = Url::parse("http://example.com/page");
    REQUIRE(base.has_value());
    auto r = base->resolve("?new=1");
    REQUIRE(r.has_value());
    CHECK(r->path() == "/page");
    CHECK(r->query() == "new=1");
    CHECK(r->fragment().empty());
}

TEST_CASE("Url::resolve: empty reference returns base") {
    auto base = Url::parse("http://example.com/page");
    REQUIRE(base.has_value());
    auto r = base->resolve("");
    REQUIRE(r.has_value());
    CHECK(r->to_string() == "http://example.com/page");
}

TEST_CASE("Url: equality") {
    auto a = Url::parse("http://example.com/");
    auto b = Url::parse("http://example.com/");
    auto c = Url::parse("http://other.com/");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(c.has_value());
    CHECK(*a == *b);
    CHECK_FALSE(*a == *c);
}

TEST_CASE("percent_decode: basic") {
    CHECK(percent_decode("Hello%20World") == "Hello World");
    CHECK(percent_decode("foo+bar") == "foo bar");
    CHECK(percent_decode("no-encoding") == "no-encoding");
    CHECK(percent_decode("%2F") == "/");
    CHECK(percent_decode("%41%42%43") == "ABC");
}

TEST_CASE("percent_decode: incomplete sequence passes through") {
    CHECK(percent_decode("a%2") == "a%2");
    CHECK(percent_decode("a%") == "a%");
}

TEST_CASE("percent_encode: unreserved chars pass through") {
    CHECK(percent_encode("abc-._~") == "abc-._~");
    CHECK(percent_encode("ABC123") == "ABC123");
}

TEST_CASE("percent_encode: special chars encoded") {
    CHECK(percent_encode(" ") == "%20");
    CHECK(percent_encode("/") == "%2F");
    CHECK(percent_encode("a b/c") == "a%20b%2Fc");
}

TEST_CASE("percent_encode / percent_decode: round-trip") {
    const std::string orig = "Hello World! /path?query=value&key=other";
    CHECK(percent_decode(percent_encode(orig)) == orig);
}
