#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <utility>
#include <vector>

using tvshow::layout::Box;
using tvshow::layout::layout;
using tvshow::layout::Viewport;
using tvshow::render::CharGrid;
using tvshow::style::StyledNode;

// ── helpers ──────────────────────────────────────────────────────────────────

// StyledNode::node points into the dom::Document, and layout::Box::node
// points into the StyledNode tree. Both the Document and the StyledNode tree
// must already be in their final resting place before layout() runs over
// them, otherwise Box::node ends up dangling once the temporary holding the
// tree is moved/destroyed. So this helper only builds doc+tree (returned via
// a guaranteed-elided prvalue); each test calls layout() itself, after the
// structured binding has settled the tree into its final stack slot.
struct DocTree {
    tvshow::dom::Document doc;
    StyledNode tree;
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static DocTree make_tree(std::string_view html, std::string_view css = "") {
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

// ── basic geometry ────────────────────────────────────────────────────────────

TEST_CASE("render: grid size matches root border box") {
    auto [doc, tree] = make_tree("<body></body>");
    const Box box = layout(tree, {20, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.cols() == 20);
    CHECK(grid.rows() == 5);
}

TEST_CASE("render: default cells are blank space") {
    auto [doc, tree] = make_tree("<body></body>");
    const Box box = layout(tree, {10, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U' ');
}

// ── background ───────────────────────────────────────────────────────────────

TEST_CASE("render: background-color fills the border box") {
    // The div has no content, so without an explicit height it would
    // shrink-wrap to zero rows (correct CSS auto-height behavior) and there
    // would be nothing to paint.
    auto [doc, tree] =
        make_tree("<body><div></div></body>", "div { background-color: #ff0000; height: 3em; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.bg == 0xFF0000U);
    CHECK(grid.at({9, 0}).attr.bg == 0xFF0000U);
}

// ── border ───────────────────────────────────────────────────────────────────

TEST_CASE("render: border-style:solid draws box-drawing corners") {
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { border-style: solid; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'┌');
    CHECK(grid.at({9, 0}).cp == U'┐');
}

// ── text ─────────────────────────────────────────────────────────────────────

TEST_CASE("render: text node paints characters into content box") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>");
    const Box box = layout(tree, {20, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'h');
    CHECK(grid.at({1, 0}).cp == U'i');
}

TEST_CASE("render: text wraps at content box width") {
    auto [doc, tree] = make_tree("<body><p>abcdefghij</p></body>");
    const Box box = layout(tree, {5, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'a');
    CHECK(grid.at({4, 0}).cp == U'e');
    CHECK(grid.at({0, 1}).cp == U'f');
}

TEST_CASE("render: color property sets foreground of painted text") {
    auto [doc, tree] = make_tree("<body><p>x</p></body>", "p { color: #00ff00; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.fg == 0x00FF00U);
}

TEST_CASE("render: font-weight:bold sets bold attribute") {
    auto [doc, tree] = make_tree("<body><p>x</p></body>", "p { font-weight: bold; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.bold);
}

TEST_CASE("render: visibility:hidden suppresses painting") {
    auto [doc, tree] = make_tree("<body><div></div></body>",
                                 "div { background-color: #ff0000; visibility: hidden; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.bg != 0xFF0000U);
}
