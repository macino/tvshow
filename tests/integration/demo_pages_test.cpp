#include "tvshow/app/page.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/cpp_http_client.hpp"
#include "tvshow/net/http_client.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/util/url.hpp"

#include <doctest/doctest.h>

#include <variant>

#include "server_fixture.hpp"

using tvshow::app::load_page;
using tvshow::app::post_page;
using tvshow::itest::ServerFixture;
using tvshow::layout::collect_links;
using tvshow::layout::Viewport;

TEST_CASE("integration: landing page lists sample pages") {
    const ServerFixture server;
    auto page = load_page(server.base_url() + "/", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "tvshow demo server");
    const auto links = collect_links(page->box);
    CHECK(links.size() >= 9);
}

TEST_CASE("integration: typography page parses and lays out") {
    const ServerFixture server;
    auto page = load_page(server.base_url() + "/pages/typography.html", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Typography");
}

TEST_CASE("integration: theme CSS applies different background colors") {
    const ServerFixture server;
    auto tv = load_page(server.base_url() + "/pages/themes.html", Viewport{80, 25});
    auto dk = load_page(server.base_url() + "/pages/themes-dark.html", Viewport{80, 25});
    auto lt = load_page(server.base_url() + "/pages/themes-light.html", Viewport{80, 25});
    REQUIRE(tv.has_value());
    REQUIRE(dk.has_value());
    REQUIRE(lt.has_value());

    CHECK(tv->sheets.size() >= 1);
    CHECK(dk->sheets.size() >= 1);
    CHECK(lt->sheets.size() >= 1);

    const auto grid_tv = tvshow::render::render(tv->box);
    const auto grid_dk = tvshow::render::render(dk->box);
    const auto grid_lt = tvshow::render::render(lt->box);

    const auto bg_tv = grid_tv.at({5, 5}).attr.bg;
    const auto bg_dk = grid_dk.at({5, 5}).attr.bg;
    const auto bg_lt = grid_lt.at({5, 5}).attr.bg;

    CHECK(bg_tv != bg_dk);
    CHECK(bg_tv != bg_lt);
    CHECK(bg_dk != bg_lt);
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

TEST_CASE("integration: post_page submits form and renders response") {
    const ServerFixture server;
    const std::string echo_url = server.base_url() + "/echo";
    const auto page = post_page(echo_url, "user=alice&qty=3", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Echo");
    // The rendered echo page should mention the submitted fields.
    CHECK(page->doc.root != nullptr);
}

TEST_CASE("integration: GET form submission appends query to echo URL") {
    const ServerFixture server;
    // Simulate what BrowserView.submit_form does for GET forms.
    const std::string get_url = server.base_url() + "/echo?color=blue&size=L";
    const auto page = load_page(get_url, Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Echo");
}

TEST_CASE("integration: two-tab state isolation") {
    const ServerFixture server;
    auto tab1 = load_page(server.base_url() + "/", Viewport{80, 24});
    auto tab2 = load_page(server.base_url() + "/pages/typography.html", Viewport{80, 24});
    REQUIRE(tab1.has_value());
    REQUIRE(tab2.has_value());

    // Each tab holds its own independent URL.
    CHECK(tab1->url != tab2->url);
    const std::string tab2_url = tab2->url;

    // Simulating tab1 navigating forward does not disturb tab2.
    tab1 = load_page(server.base_url() + "/pages/typography.html", Viewport{80, 24});
    REQUIRE(tab1.has_value());
    CHECK(tab2->url == tab2_url);

    // Direct mutation of tab1 is invisible to tab2.
    tab1->url = "mutated";
    CHECK(tab2->url == tab2_url);
}

TEST_CASE("integration: https request fails fast with an internal error page") {
    auto page = load_page("https://127.0.0.1:1/pages/colors.html", Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Network Error");
}

// M13: address bar validates URL before navigating — mirrors what the
// Ctrl-L handler does (Url::parse gate, then load_page).
TEST_CASE("integration: address bar accepts valid URL and navigates") {
    const ServerFixture server;
    const std::string typed = server.base_url() + "/pages/typography.html";

    // Simulate the dialog validation step.
    REQUIRE(tvshow::util::Url::parse(typed).has_value());
    CHECK_FALSE(tvshow::util::Url::parse("not-a-url").has_value());
    CHECK_FALSE(tvshow::util::Url::parse("").has_value());

    // Simulate navigation after validation.
    const auto page = load_page(typed, Viewport{80, 24});
    REQUIRE(page.has_value());
    CHECK(page->doc.title == "Typography");
}
