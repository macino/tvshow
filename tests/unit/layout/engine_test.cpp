#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/types.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using tvshow::layout::Box;
using tvshow::layout::layout;
using tvshow::layout::Viewport;
using tvshow::style::StyledNode;

// ── helpers ──────────────────────────────────────────────────────────────────

// StyledNode::node points into the dom::Document, so the Document must
// outlive the StyledNode tree — bundle them together to avoid dangling
// pointers once the helper returns.
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

// NOLINTNEXTLINE(misc-no-recursion)
static const Box* find_box(const Box& root, std::string_view tag) {
    if (root.node != nullptr && root.node->node != nullptr && root.node->node->tag == tag) {
        return &root;
    }
    for (const auto& child : root.children) {
        const auto* found = find_box(child, tag);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

// ── basic geometry ────────────────────────────────────────────────────────────

TEST_CASE("layout: root box starts at (0,0)") {
    auto [doc, tree] = make_tree("<body></body>");
    const Box root = layout(tree, {80, 24});
    CHECK(root.border_box.origin.col == 0);
    CHECK(root.border_box.origin.row == 0);
}

TEST_CASE("layout: root box width equals viewport width") {
    auto [doc, tree] = make_tree("<body></body>");
    const Box root = layout(tree, {80, 24});
    CHECK(root.border_box.size.cols == 80);
}

TEST_CASE("layout: block child stretches to parent content width") {
    auto [doc, tree] = make_tree("<body><div></div></body>");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->border_box.size.cols == 80);
}

TEST_CASE("layout: two block children stack vertically") {
    auto [doc, tree] = make_tree("<body><div></div><div></div></body>");
    const Box root = layout(tree, {80, 24});
    const auto* body = find_box(root, "body");
    REQUIRE(body != nullptr);
    REQUIRE(body->children.size() == 2);
    const int y0 = body->children[0].border_box.origin.row;
    const int y1 = body->children[1].border_box.origin.row;
    CHECK(y1 >= y0);
    // second div starts where first ends
    CHECK(y1 == y0 + body->children[0].border_box.size.rows);
}

TEST_CASE("layout: display:none removes element from box tree") {
    auto [doc, tree] = make_tree("<body><div></div><p></p></body>", "div { display: none; }");
    const Box root = layout(tree, {80, 24});
    const auto* body = find_box(root, "body");
    REQUIRE(body != nullptr);
    // Only <p> should be a box child (div hidden)
    CHECK(find_box(root, "div") == nullptr);
    CHECK(find_box(root, "p") != nullptr);
}

TEST_CASE("layout: explicit width in ch sets content width") {
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { width: 10ch; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->content_box.size.cols == 10);
}

TEST_CASE("layout: padding shrinks content box") {
    // padding: 2ch → 2 cols on each side, content_width = 80 - 4
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { padding: 2ch; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->content_box.size.cols == 76);
    CHECK(div->content_box.origin.col == 2);
}

TEST_CASE("layout: border-style:solid adds 1-cell border on each side") {
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { border-style: solid; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->content_box.size.cols == 78);  // 80 - 2 borders
    CHECK(div->content_box.origin.col == 1);  // 1 cell border on left
    CHECK(div->content_box.origin.row >= 1);  // 1 cell border on top
}

TEST_CASE("layout: explicit height on block container is ignored (text browser)") {
    // Text browsers drive block height from content, not CSS height — explicit height
    // would create blank rows with no content.  img/form controls are the exception.
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { height: 5em; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->content_box.size.rows == 0);
}

TEST_CASE("layout: text content generates positive height") {
    // 80 characters of text in a 10-col viewport → at least 8 rows
    auto [doc, tree] = make_tree("<body><p>aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa "
                                 "aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa</p></body>");
    const Box root = layout(tree, {10, 100});
    const auto* p = find_box(root, "p");
    REQUIRE(p != nullptr);
    CHECK(p->content_box.size.rows >= 8);
}

TEST_CASE("layout: block child positioned inside parent content box") {
    auto [doc, tree] = make_tree("<body><div></div></body>", "body { padding: 1ch; }");
    const Box root = layout(tree, {80, 24});
    const auto* body = find_box(root, "body");
    const auto* div = find_box(root, "div");
    REQUIRE(body != nullptr);
    REQUIRE(div != nullptr);
    CHECK(div->border_box.origin.col == body->content_box.origin.col);
    CHECK(div->border_box.origin.row == body->content_box.origin.row);
}

// ── table column alignment ──────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
static std::vector<const Box*> find_all_boxes(const Box& root, std::string_view tag) {
    std::vector<const Box*> out;
    if (root.node != nullptr && root.node->node != nullptr && root.node->node->tag == tag) {
        out.push_back(&root);
    }
    for (const auto& child : root.children) {
        auto sub = find_all_boxes(child, tag);
        out.insert(out.end(), sub.begin(), sub.end());
    }
    return out;
}

TEST_CASE("layout: table cells align to max column width across rows") {
    auto [doc, tree] = make_tree(
        "<body><table>"
        "<tr><td>Hello</td><td>Hi</td></tr>"
        "<tr><td>X</td><td>World!</td></tr>"
        "</table></body>");
    const Box root = layout(tree, {80, 24});

    // gumbo auto-inserts <tbody>, so find rows by tag.
    const auto rows = find_all_boxes(root, "tr");
    REQUIRE(rows.size() >= 2);
    REQUIRE(rows[0]->children.size() >= 2);
    REQUIRE(rows[1]->children.size() >= 2);

    // Cell widths should match across rows (same column = same width).
    CHECK(rows[0]->children[0].border_box.size.cols == rows[1]->children[0].border_box.size.cols);
    CHECK(rows[0]->children[1].border_box.size.cols == rows[1]->children[1].border_box.size.cols);
}

// ── position: relative ──────────────────────────────────────────────────────

TEST_CASE("layout: position:relative offsets box from normal-flow position") {
    auto [doc, tree] = make_tree("<body><div></div></body>",
                                 "div { position: relative; left: 16px; top: 16px; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->border_box.origin.col == 2);  // 16px / 8 = 2 cols
    CHECK(div->border_box.origin.row == 1);  // 16px / 16 = 1 row
}

TEST_CASE("layout: position:relative with right/bottom offsets in negative direction") {
    auto [doc, tree] = make_tree(
        "<body><div style='padding-top: 32px; padding-left: 16px;'>"
        "<p>x</p></div></body>",
        "p { position: relative; right: 8px; bottom: 16px; }");
    const Box root = layout(tree, {80, 24});
    const auto* p = find_box(root, "p");
    REQUIRE(p != nullptr);
    CHECK(p->border_box.origin.col == 1);  // 16px pad / 8 = col 2, minus right 8px/8 = 1 → col 1
    CHECK(p->border_box.origin.row == 1);  // 32px pad / 16 = row 2, minus bottom 16px/16 = 1 → row 1
}

// ── position: absolute ──────────────────────────────────────────────────────

TEST_CASE("layout: position:absolute places box relative to parent content origin") {
    auto [doc, tree] = make_tree(
        "<body><div style='position: relative; padding: 16px;'>"
        "<p>x</p></div></body>",
        "p { position: absolute; left: 8px; top: 16px; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    const auto* p = find_box(root, "p");
    REQUIRE(div != nullptr);
    REQUIRE(p != nullptr);
    CHECK(p->border_box.origin.col == div->content_box.origin.col + 1);  // left: 8px = 1ch
    CHECK(p->border_box.origin.row == div->content_box.origin.row + 1);  // top: 16px = 1row
}
