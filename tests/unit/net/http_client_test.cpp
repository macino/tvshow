#include "tvshow/net/fake_http_client.hpp"
#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <utility>
#include <variant>

using tvshow::net::FakeHttpClient;
using tvshow::net::Headers;
using tvshow::net::NetworkError;
using tvshow::net::Response;
using tvshow::net::Result;
using tvshow::util::Url;

static Url parse(const char* s) {
    auto u = Url::parse(s);
    REQUIRE(u.has_value());
    return *u;
}

TEST_CASE("Response::content_type: parses without params") {
    Response r;
    r.headers["content-type"] = "text/html";
    CHECK(r.content_type() == "text/html");
}

TEST_CASE("Response::content_type: strips charset param") {
    Response r;
    r.headers["content-type"] = "text/html; charset=utf-8";
    CHECK(r.content_type() == "text/html");
}

TEST_CASE("Response::charset: from Content-Type") {
    Response r;
    r.headers["content-type"] = "text/html; charset=iso-8859-1";
    CHECK(r.charset() == "iso-8859-1");
}

TEST_CASE("Response::charset: defaults to utf-8 when absent") {
    Response r;
    r.headers["content-type"] = "text/html";
    CHECK(r.charset() == "utf-8");
}

TEST_CASE("Response::charset: defaults to utf-8 when no Content-Type") {
    const Response r;
    CHECK(r.charset() == "utf-8");
}

TEST_CASE("FakeHttpClient: registered GET returns response") {
    FakeHttpClient client;
    Response resp;
    resp.status = 200;
    resp.body = "hello";
    resp.headers["content-type"] = "text/plain";
    client.on("http://example.com/", std::move(resp));

    const auto result = client.get(parse("http://example.com/"));
    const auto* r = std::get_if<Response>(&result);
    REQUIRE(r != nullptr);
    CHECK(r->status == 200);
    CHECK(r->body == "hello");
}

TEST_CASE("FakeHttpClient: unregistered URL returns NetworkError") {
    FakeHttpClient client;
    const auto result = client.get(parse("http://example.com/missing"));
    CHECK(std::holds_alternative<NetworkError>(result));
}

TEST_CASE("FakeHttpClient: redirect chain followed") {
    FakeHttpClient client;

    Response redir;
    redir.status = 301;
    redir.headers["location"] = "http://example.com/final";
    client.on("http://example.com/start", std::move(redir));

    Response final_resp;
    final_resp.status = 200;
    final_resp.body = "arrived";
    client.on("http://example.com/final", std::move(final_resp));

    const auto result = client.get(parse("http://example.com/start"));
    const auto* r = std::get_if<Response>(&result);
    REQUIRE(r != nullptr);
    CHECK(r->status == 200);
    CHECK(r->body == "arrived");
}

TEST_CASE("FakeHttpClient: max_redirects=0 stops at first redirect") {
    FakeHttpClient client;
    Response redir;
    redir.status = 302;
    redir.headers["location"] = "http://example.com/final";
    client.on("http://example.com/start", std::move(redir));

    const auto result = client.get(parse("http://example.com/start"), 0);
    const auto* r = std::get_if<Response>(&result);
    REQUIRE(r != nullptr);
    CHECK(r->status == 302);
}

TEST_CASE("FakeHttpClient: handler lambda") {
    FakeHttpClient client;
    client.on("http://example.com/echo", [](const tvshow::util::Url& u) -> Result {
        Response r;
        r.status = 200;
        r.body = u.to_string();
        return r;
    });

    const auto result = client.get(parse("http://example.com/echo"));
    const auto* r = std::get_if<Response>(&result);
    REQUIRE(r != nullptr);
    CHECK(r->body == "http://example.com/echo");
}

TEST_CASE("FakeHttpClient: POST dispatches to registered route") {
    FakeHttpClient client;
    Response resp;
    resp.status = 201;
    client.on("http://example.com/submit", std::move(resp));

    const auto result = client.post(parse("http://example.com/submit"), "key=value");
    const auto* r = std::get_if<Response>(&result);
    REQUIRE(r != nullptr);
    CHECK(r->status == 201);
}

// Table-driven: all redirect status codes are followed.
TEST_CASE("FakeHttpClient: all redirect statuses followed") {
    struct TC {
        int status;
    };
    const std::array<TC, 5> cases = {{{301}, {302}, {303}, {307}, {308}}};

    for (const auto& tc : cases) {
        INFO("status=" << tc.status);
        FakeHttpClient client;
        Response redir;
        redir.status = tc.status;
        redir.headers["location"] = "http://example.com/dest";
        client.on("http://example.com/src", std::move(redir));

        Response dest;
        dest.status = 200;
        client.on("http://example.com/dest", std::move(dest));

        const auto result = client.get(parse("http://example.com/src"));
        const auto* r = std::get_if<Response>(&result);
        REQUIRE(r != nullptr);
        CHECK(r->status == 200);
    }
}
