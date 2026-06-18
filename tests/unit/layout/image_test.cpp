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

// ── img cell sizing ───────────────────────────────────────────────────────────

TEST_CASE("img: width and height attrs map to cells (px)") {
    // 64px wide → 8 cols (1px = 1/8 col), 32px tall → 2 rows (1px = 1/16 row)
    auto [doc, tree] = make_tree(
        R"(<body><img src="x.png" alt="photo" width="64" height="32"></body>)");
    const Box root = layout(tree, {80, 24});
    const auto* img = find_box(root, "img");
    REQUIRE(img != nullptr);
    CHECK(img->border_box.size.cols == 8);
    CHECK(img->border_box.size.rows == 2);
}

TEST_CASE("img: no attrs default to alt-text width and 1 row") {
    // alt="hi" → "[hi]" = 4 chars wide, 1 row
    auto [doc, tree] = make_tree(R"(<body><img src="x.png" alt="hi"></body>)");
    const Box root = layout(tree, {80, 24});
    const auto* img = find_box(root, "img");
    REQUIRE(img != nullptr);
    CHECK(img->border_box.size.cols == 4);
    CHECK(img->border_box.size.rows == 1);
}

TEST_CASE("img: no alt and no size defaults to minimum 1x1") {
    auto [doc, tree] = make_tree(R"(<body><img src="x.png"></body>)");
    const Box root = layout(tree, {80, 24});
    const auto* img = find_box(root, "img");
    REQUIRE(img != nullptr);
    CHECK(img->border_box.size.cols >= 1);
    CHECK(img->border_box.size.rows >= 1);
}

TEST_CASE("img: participates in block layout (contributes to parent height)") {
    // img with height 16px → 1 row; parent block should grow to contain it
    auto [doc, tree] = make_tree(
        R"(<body><div><img src="x.png" alt="x" width="32" height="16"></div></body>)");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    REQUIRE(div != nullptr);
    CHECK(div->border_box.size.rows >= 1);
}

TEST_CASE("img: width-only attr: height defaults to 1 row") {
    // width="80" → 10 cols; no height → 1 row
    auto [doc, tree] = make_tree(R"(<body><img src="x.png" alt="hi" width="80"></body>)");
    const Box root = layout(tree, {80, 24});
    const auto* img = find_box(root, "img");
    REQUIRE(img != nullptr);
    CHECK(img->border_box.size.cols == 10);
    CHECK(img->border_box.size.rows == 1);
}
