#include "tvshow/net/blocklist.hpp"

#include <doctest/doctest.h>

using tvshow::net::Blocklist;
using tvshow::net::glob_match;
using tvshow::net::is_blocked;
using tvshow::net::parse_blocklist;

// ── glob_match ───────────────────────────────────────────────────────────────

TEST_CASE("glob_match: exact match") {
    CHECK(glob_match("abc", "abc"));
    CHECK_FALSE(glob_match("abc", "abd"));
}

TEST_CASE("glob_match: trailing star matches any suffix") {
    CHECK(glob_match("http://ads.example.com/*", "http://ads.example.com/banner.js"));
    CHECK(glob_match("http://ads.example.com/*", "http://ads.example.com/"));
    CHECK_FALSE(glob_match("http://ads.example.com/*", "http://example.com/"));
}

TEST_CASE("glob_match: leading star matches any prefix") {
    CHECK(glob_match("*/ads/*", "http://example.com/ads/banner.js"));
}

TEST_CASE("glob_match: question mark matches exactly one char") {
    CHECK(glob_match("a?c", "abc"));
    CHECK_FALSE(glob_match("a?c", "ac"));
    CHECK_FALSE(glob_match("a?c", "abbc"));
}

TEST_CASE("glob_match: multiple stars") {
    CHECK(glob_match("*ads*track*", "http://x.com/ads/track/1"));
}

TEST_CASE("glob_match: empty pattern only matches empty text") {
    CHECK(glob_match("", ""));
    CHECK_FALSE(glob_match("", "x"));
}

// ── parse_blocklist ──────────────────────────────────────────────────────────

TEST_CASE("parse_blocklist: parses block and hide rules") {
    const auto list = parse_blocklist(
        "block: http://ads.example.com/*\n"
        "hide: .ad-banner\n");
    REQUIRE(list.block_globs.size() == 1);
    CHECK(list.block_globs[0] == "http://ads.example.com/*");
    REQUIRE(list.hide_selectors.size() == 1);
    CHECK(list.hide_selectors[0] == ".ad-banner");
}

TEST_CASE("parse_blocklist: ignores blank lines and comments") {
    const auto list = parse_blocklist("\n# comment\n\nblock: *tracker*\n");
    REQUIRE(list.block_globs.size() == 1);
    CHECK(list.block_globs[0] == "*tracker*");
}

TEST_CASE("parse_blocklist: ignores unknown-prefix lines") {
    const auto list = parse_blocklist("allow: something\nblock: *x*\n");
    CHECK(list.block_globs.size() == 1);
}

TEST_CASE("parse_blocklist: empty text yields empty Blocklist") {
    const auto list = parse_blocklist("");
    CHECK(list.empty());
}

// ── is_blocked ───────────────────────────────────────────────────────────────

TEST_CASE("is_blocked: matches any glob in the list") {
    Blocklist list;
    list.block_globs = {"*ads*", "*tracker*"};
    CHECK(is_blocked(list, "http://x.com/ads/1"));
    CHECK(is_blocked(list, "http://x.com/tracker.js"));
    CHECK_FALSE(is_blocked(list, "http://x.com/content.html"));
}

TEST_CASE("is_blocked: empty list blocks nothing") {
    Blocklist list;
    CHECK_FALSE(is_blocked(list, "http://x.com/anything"));
}
