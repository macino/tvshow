#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <utility>
#include <vector>

using tvshow::layout::Box;
using tvshow::layout::collect_links;
using tvshow::layout::find_anchor_row;
using tvshow::layout::layout;
using tvshow::layout::Viewport;
using tvshow::style::StyledNode;

namespace {

struct DocTree {
    tvshow::dom::Document doc;
    StyledNode tree;
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
DocTree make_tree(std::string_view html, std::string_view css = "") {
    auto doc = tvshow::dom::parse(html);
    REQUIRE(doc.has_value());
    std::vector<tvshow::css::Stylesheet> sheets;
    if (!css.empty()) {
        auto sheet = tvshow::css::parse(css);
        if (sheet.has_value()) {
            sheets.push_back(std::move(*sheet));
        }
    }
    auto tree = tvshow::style::resolve(*doc, sheets);
    REQUIRE(tree.has_value());
    return {std::move(*doc), std::move(*tree)};
}

}  // namespace

TEST_CASE("collect_links: no links yields no entries") {
    auto [doc, tree] = make_tree("<body><p>plain text</p></body>");
    const Box box = layout(tree, {20, 5});
    CHECK(collect_links(box).empty());
}

TEST_CASE("collect_links: one link in flowing text yields one entry") {
    auto [doc, tree] = make_tree("<body><p>see <a href=\"/x\">this</a> page</p></body>");
    const Box box = layout(tree, {20, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 1);
    CHECK(links[0].href == "/x");
    REQUIRE(links[0].spans.size() == 1);
    // "see " is 4 cols wide, so "this" starts at col 4, row 0, width 4.
    CHECK(links[0].spans[0].origin.col == 4);
    CHECK(links[0].spans[0].origin.row == 0);
    CHECK(links[0].spans[0].size.cols == 4);
    CHECK(links[0].spans[0].size.rows == 1);
}

TEST_CASE("collect_links: two separate links yield two entries in reading order") {
    auto [doc, tree] =
        make_tree(R"(<body><p><a href="/a">one</a> <a href="/b">two</a></p></body>)");
    const Box box = layout(tree, {20, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 2);
    CHECK(links[0].href == "/a");
    CHECK(links[1].href == "/b");
}

// ── iframe/video/audio link-out placeholders (SPEC Q-30) ────────────────────

TEST_CASE("collect_links: iframe[src] yields a focusable [Embedded: ...] link") {
    auto [doc, tree] = make_tree(R"(<body><iframe src="https://example.com/x"></iframe></body>)");
    const Box box = layout(tree, {80, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 1);
    CHECK(links[0].href == "https://example.com/x");
}

TEST_CASE("collect_links: video[src] yields a focusable [Media: ...] link") {
    auto [doc, tree] = make_tree(R"(<body><video src="movie.mp4"></video></body>)");
    const Box box = layout(tree, {80, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 1);
    CHECK(links[0].href == "movie.mp4");
}

TEST_CASE("collect_links: audio with no own src falls back to first <source src>") {
    auto [doc, tree] = make_tree(
        R"(<body><audio><source src="a.ogg"><source src="b.mp3"></audio></body>)");
    const Box box = layout(tree, {80, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 1);
    CHECK(links[0].href == "a.ogg");
}

TEST_CASE("collect_links: iframe with no src attribute contributes no link") {
    auto [doc, tree] = make_tree("<body><iframe></iframe></body>");
    const Box box = layout(tree, {80, 5});
    CHECK(collect_links(box).empty());
}

// ── find_anchor_row ───────────────────────────────────────────────────────────

TEST_CASE("find_anchor_row: returns -1 when no element has matching id") {
    auto [doc, tree] = make_tree("<body><p>no anchors here</p></body>");
    const Box box = layout(tree, {40, 10});
    CHECK(find_anchor_row(box, "nowhere") == -1);
}

TEST_CASE("find_anchor_row: returns row of element with matching id") {
    // First p is at row 0 (1 line), second p at row 1.
    auto [doc, tree] = make_tree("<body><p>first</p><p id=\"target\">second</p></body>");
    const Box box = layout(tree, {40, 10});
    const int row = find_anchor_row(box, "target");
    CHECK(row >= 1);  // second p comes after first p
}

TEST_CASE("find_anchor_row: element not found returns -1") {
    auto [doc, tree] = make_tree("<body><div id=\"foo\">content</div></body>");
    const Box box = layout(tree, {40, 10});
    CHECK(find_anchor_row(box, "bar") == -1);
}

TEST_CASE("collect_links: a link wrapped across lines yields one entry with multiple spans") {
    // Width 5 forces "linktext" (8 chars) to hard-wrap across two rows.
    auto [doc, tree] = make_tree("<body><p><a href=\"/x\">linktext</a></p></body>");
    const Box box = layout(tree, {5, 5});
    const auto links = collect_links(box);
    REQUIRE(links.size() == 1);
    CHECK(links[0].href == "/x");
    REQUIRE(links[0].spans.size() == 2);
    CHECK(links[0].spans[0].origin.row == 0);
    CHECK(links[0].spans[1].origin.row == 1);
}
