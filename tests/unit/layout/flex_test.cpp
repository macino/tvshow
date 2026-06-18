#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using tvshow::layout::Box;
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

// NOLINTNEXTLINE(misc-no-recursion)
const Box* find_box(const Box& root, std::string_view tag) {
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

}  // namespace

// ── Row direction: justify-content ───────────────────────────────────────────

TEST_CASE("flex row flex-start: items packed at left") {
    auto [doc, tree] =
        make_tree("<body><div id=c><span></span><span></span></div></body>",
                  "div { display:flex; width:30ch; } span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.col == 0);
    CHECK(cont->children[1].border_box.origin.col == 5);
}

TEST_CASE("flex row flex-end: items packed at right") {
    auto [doc, tree] = make_tree("<body><div><span></span><span></span></div></body>",
                                 "div { display:flex; justify-content:flex-end; width:30ch; } "
                                 "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.col == 20);
    CHECK(cont->children[1].border_box.origin.col == 25);
}

TEST_CASE("flex row center: items centered") {
    auto [doc, tree] = make_tree("<body><div><span></span><span></span></div></body>",
                                 "div { display:flex; justify-content:center; width:30ch; } "
                                 "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    // free=20, start=10 → items at 10 and 15
    CHECK(cont->children[0].border_box.origin.col == 10);
    CHECK(cont->children[1].border_box.origin.col == 15);
}

TEST_CASE("flex row space-between: equal gap between items") {
    auto [doc, tree] = make_tree("<body><div><span></span><span></span></div></body>",
                                 "div { display:flex; justify-content:space-between; width:30ch; } "
                                 "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.col == 0);
    CHECK(cont->children[1].border_box.origin.col == 25);
}

TEST_CASE("flex row space-around: equal space around items") {
    // 2 items × 5ch = 10ch used, free = 20, around: 10 per item → 5 each side
    auto [doc, tree] = make_tree("<body><div><span></span><span></span></div></body>",
                                 "div { display:flex; justify-content:space-around; width:30ch; } "
                                 "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.col == 5);
    CHECK(cont->children[1].border_box.origin.col == 20);
}

TEST_CASE("flex row gap: gap between items") {
    // gap:2ch, 2 items of 5ch → item0 at 0, item1 at 7
    auto [doc, tree] =
        make_tree("<body><div><span></span><span></span></div></body>",
                  "div { display:flex; gap:2ch; width:30ch; } span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.col == 0);
    CHECK(cont->children[1].border_box.origin.col == 7);
}

// ── flex-grow ─────────────────────────────────────────────────────────────────

TEST_CASE("flex row flex-grow: item grows to fill container") {
    // 1 item, flex-grow:1, base 0 → item fills all 20ch
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; width:20ch; height:3em; } span { flex-grow:1; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    CHECK(cont->children[0].border_box.size.cols == 20);
}

TEST_CASE("flex row flex-grow proportional: two items grow equally") {
    // 2 items, flex-grow:1 each, base 0 → each gets 15ch
    auto [doc, tree] =
        make_tree("<body><div><span></span><span></span></div></body>",
                  "div { display:flex; width:30ch; height:3em; } span { flex-grow:1; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.size.cols == 15);
    CHECK(cont->children[1].border_box.size.cols == 15);
}

TEST_CASE("flex row flex-basis: explicit base size") {
    // flex-basis:10ch, container 30ch, no grow → item width = 10
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; width:30ch; height:3em; } span { flex-basis:10ch; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    CHECK(cont->children[0].border_box.size.cols == 10);
}

// ── align-items ───────────────────────────────────────────────────────────────

TEST_CASE("flex row align-items stretch: items stretch to container height") {
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; width:20ch; height:5em; } span { width:5ch; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    CHECK(cont->children[0].border_box.size.rows == 5);
}

TEST_CASE("flex row align-items flex-start: items at top of container") {
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; align-items:flex-start; width:20ch; height:5em; } "
                  "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    CHECK(cont->children[0].border_box.origin.row == cont->content_box.origin.row);
}

TEST_CASE("flex row align-items flex-end: items at bottom of container") {
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; align-items:flex-end; width:20ch; height:5em; } "
                  "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    // item bottom = container bottom
    const int item_bottom =
        cont->children[0].border_box.origin.row + cont->children[0].border_box.size.rows;
    const int cont_bottom = cont->content_box.origin.row + cont->content_box.size.rows;
    CHECK(item_bottom == cont_bottom);
}

TEST_CASE("flex row align-items center: items vertically centered") {
    auto [doc, tree] =
        make_tree("<body><div><span></span></div></body>",
                  "div { display:flex; align-items:center; width:20ch; height:6em; } "
                  "span { width:5ch; height:2em; }");
    const Box root = layout(tree, {40, 20});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 1);
    // free cross = 4 rows, item starts at content_row + 2
    CHECK(cont->children[0].border_box.origin.row == cont->content_box.origin.row + 2);
}

// ── Column direction ──────────────────────────────────────────────────────────

TEST_CASE("flex column flex-start: items stack vertically from top") {
    auto [doc, tree] =
        make_tree("<body><div><span></span><span></span></div></body>",
                  "div { display:flex; flex-direction:column; width:20ch; height:20em; } "
                  "span { height:3em; }");
    const Box root = layout(tree, {40, 30});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.row == cont->content_box.origin.row);
    CHECK(cont->children[1].border_box.origin.row == cont->content_box.origin.row + 3);
}

TEST_CASE("flex column gap: gap between column items") {
    auto [doc, tree] =
        make_tree("<body><div><span></span><span></span></div></body>",
                  "div { display:flex; flex-direction:column; gap:2ch; width:20ch; } "
                  "span { height:3em; }");
    const Box root = layout(tree, {40, 30});
    const auto* cont = find_box(root, "div");
    REQUIRE(cont != nullptr);
    REQUIRE(cont->children.size() == 2);
    CHECK(cont->children[0].border_box.origin.row == cont->content_box.origin.row);
    CHECK(cont->children[1].border_box.origin.row == cont->content_box.origin.row + 5);
}
