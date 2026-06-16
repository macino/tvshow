#include "tvshow/app/page.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/cpp_http_client.hpp"
#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <doctest/doctest.h>

#include <variant>

#include "server_fixture.hpp"

using tvshow::app::load_page;
using tvshow::itest::ServerFixture;
using tvshow::layout::collect_links;
using tvshow::layout::Viewport;

TEST_CASE("integration: landing page lists sample pages") {
    const ServerFixture server;
    auto page = load_page(server.base_url() + "/", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "tvshow demo server");
    const auto links = collect_links(page->box);
    CHECK(links.size() == 5);
}

TEST_CASE("integration: typography page parses and lays out") {
    const ServerFixture server;
    auto page = load_page(server.base_url() + "/pages/typography.html", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Typography");
}

TEST_CASE("integration: 404 route renders an internal error page") {
    const ServerFixture server;
    auto page = load_page(server.base_url() + "/pages/errors/404", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "HTTP 404");
}

TEST_CASE("integration: form POST round-trips through /echo") {
    const ServerFixture server;
    tvshow::net::CppHttpClient client;
    const auto url = tvshow::util::Url::parse(server.base_url() + "/echo");
    REQUIRE(url.has_value());
    const auto result = client.post(*url, "name=tomas&subscribe=yes");
    const auto* resp = std::get_if<tvshow::net::Response>(&result);
    REQUIRE(resp != nullptr);
    CHECK(resp->status == 200);
    CHECK(resp->body.find("name = tomas") != std::string::npos);
    CHECK(resp->body.find("subscribe = yes") != std::string::npos);
}

TEST_CASE("integration: https request fails fast with an internal error page") {
    auto page = load_page("https://127.0.0.1:1/pages/colors.html", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Network Error");
}
