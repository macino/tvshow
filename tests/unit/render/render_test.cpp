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
    auto [doc, tree] =
        make_tree("<body><div>x</div></body>", "div { background-color: #ff0000; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.bg == 0xFF0000U);
    CHECK(grid.at({9, 0}).attr.bg == 0xFF0000U);
}

// ── border ───────────────────────────────────────────────────────────────────

TEST_CASE("render: border-style:solid draws box-drawing corners") {
    // padding-top/bottom push the bottom border to row 4 of the 5-row viewport
    // (1 brd + 1 pad + 1 content + 1 pad + 1 brd = 5 rows).
    auto [doc, tree] =
        make_tree("<body><div>x</div></body>",
                  "div { border-style: solid; padding-top: 1em; padding-bottom: 1em; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'┌');
    CHECK(grid.at({9, 0}).cp == U'┐');
    CHECK(grid.at({0, 4}).cp == U'└');
    CHECK(grid.at({9, 4}).cp == U'┘');
    CHECK(grid.at({1, 0}).cp == U'─');
    CHECK(grid.at({0, 1}).cp == U'│');
}

TEST_CASE("render: border-style:double draws double-line glyphs") {
    auto [doc, tree] =
        make_tree("<body><div>x</div></body>",
                  "div { border-style: double; padding-top: 1em; padding-bottom: 1em; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'╔');
    CHECK(grid.at({9, 0}).cp == U'╗');
    CHECK(grid.at({0, 4}).cp == U'╚');
    CHECK(grid.at({9, 4}).cp == U'╝');
    CHECK(grid.at({1, 0}).cp == U'═');
    CHECK(grid.at({0, 1}).cp == U'║');
}

TEST_CASE("render: border-style:dashed draws solid corners with dashed edges") {
    auto [doc, tree] =
        make_tree("<body><div>x</div></body>", "div { border-style: dashed; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'┌');
    CHECK(grid.at({9, 0}).cp == U'┐');
    CHECK(grid.at({1, 0}).cp == U'╌');
    CHECK(grid.at({0, 1}).cp == U'╎');
}

TEST_CASE("render: border-style:dotted falls back to solid") {
    auto [doc, tree] = make_tree("<body><div></div></body>", "div { border-style: dotted; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'┌');
    CHECK(grid.at({1, 0}).cp == U'─');
}

TEST_CASE("render: border-width:0 collapses the border (no cells reserved)") {
    auto [doc, tree] =
        make_tree("<body><div>x</div></body>", "div { border-style: solid; border-width: 0; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    // With the border collapsed, the content starts at the border box's
    // origin instead of being inset by one cell for the border glyphs.
    CHECK(grid.at({0, 0}).cp == U'x');
}

TEST_CASE("render: border-color tints the border glyphs") {
    auto [doc, tree] = make_tree("<body><div></div></body>",
                                 "div { border-style: solid; border-color: #00ff00; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.fg == 0x00FF00U);
    CHECK(grid.at({1, 0}).attr.fg == 0x00FF00U);
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

TEST_CASE("render: text wraps at word boundaries, not mid-word") {
    auto [doc, tree] = make_tree("<body><p>ab cd ef</p></body>");
    const Box box = layout(tree, {5, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'a');
    CHECK(grid.at({4, 0}).cp == U'd');
    CHECK(grid.at({0, 1}).cp == U'e');
    CHECK(grid.at({1, 1}).cp == U'f');
}

TEST_CASE("render: text-align:center centers each line in the content box") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>", "p { text-align: center; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({4, 0}).cp == U'h');
    CHECK(grid.at({5, 0}).cp == U'i');
}

TEST_CASE("render: text-align:right right-aligns each line in the content box") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>", "p { text-align: right; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({8, 0}).cp == U'h');
    CHECK(grid.at({9, 0}).cp == U'i');
}

TEST_CASE("render: white-space:pre preserves runs of spaces verbatim") {
    auto [doc, tree] = make_tree("<body><p>a    b</p></body>", "p { white-space: pre; }");
    const Box box = layout(tree, {20, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'a');
    CHECK(grid.at({1, 0}).cp == U' ');
    CHECK(grid.at({4, 0}).cp == U' ');
    CHECK(grid.at({5, 0}).cp == U'b');
}

TEST_CASE("render: white-space:nowrap keeps text on one line, clipped by the content box") {
    auto [doc, tree] = make_tree("<body><p>ab cd ef</p></body>", "p { white-space: nowrap; }");
    const Box box = layout(tree, {5, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'a');
    CHECK(grid.at({4, 0}).cp == U'd');
    CHECK(grid.at({0, 1}).cp == U' ');  // nothing wrapped onto row 1
}

TEST_CASE("render: visibility:hidden suppresses painting") {
    auto [doc, tree] = make_tree("<body><div></div></body>",
                                 "div { background-color: #ff0000; visibility: hidden; }");
    const Box box = layout(tree, {10, 5});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).attr.bg != 0xFF0000U);
}

// ── focus highlight ──────────────────────────────────────────────────────────

TEST_CASE("apply_focus: inverts fg/bg of cells within the given spans") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>");
    const Box box = layout(tree, {10, 5});
    CharGrid grid = tvshow::render::render(box);
    const auto before = grid.at({0, 0}).attr;
    tvshow::render::apply_focus(grid, {{{0, 0}, {2, 1}}});
    const auto after = grid.at({0, 0}).attr;
    CHECK(after.fg == before.bg);
    CHECK(after.bg == before.fg);
}

TEST_CASE("apply_focus: leaves cells outside the spans untouched") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>");
    const Box box = layout(tree, {10, 5});
    CharGrid grid = tvshow::render::render(box);
    const auto before = grid.at({5, 0}).attr;
    tvshow::render::apply_focus(grid, {{{0, 0}, {2, 1}}});
    CHECK(grid.at({5, 0}).attr == before);
}

TEST_CASE("apply_focus: clips spans that extend past the grid bounds") {
    auto [doc, tree] = make_tree("<body><p>hi</p></body>");
    const Box box = layout(tree, {10, 5});
    CharGrid grid = tvshow::render::render(box);
    // Span extends well past the 10x5 grid; should not throw and should
    // still flip the in-bounds portion.
    tvshow::render::apply_focus(grid, {{{8, 4}, {20, 20}}});
    CHECK(grid.at({9, 4}).attr.fg != 0xFFFFFFU);
}

// ── form controls ─────────────────────────────────────────────────────────────

TEST_CASE("render: text input draws brackets and initial value") {
    auto [doc, tree] = make_tree(R"(<body><input type="text" value="hi"></body>)");
    const Box box = layout(tree, {30, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({1, 0}).cp == U'h');
    CHECK(grid.at({2, 0}).cp == U'i');
    CHECK(grid.at({21, 0}).cp == U']');
}

TEST_CASE("render: text input with empty value draws blank field") {
    auto [doc, tree] = make_tree(R"(<body><input type="text"></body>)");
    const Box box = layout(tree, {30, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({1, 0}).cp == U' ');
    CHECK(grid.at({21, 0}).cp == U']');
}

TEST_CASE("render: checkbox unchecked draws [ ]") {
    auto [doc, tree] = make_tree(R"(<body><input type="checkbox"></body>)");
    const Box box = layout(tree, {10, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({1, 0}).cp == U' ');
    CHECK(grid.at({2, 0}).cp == U']');
}

TEST_CASE("render: checkbox checked draws [x]") {
    auto [doc, tree] = make_tree(R"(<body><input type="checkbox" checked></body>)");
    const Box box = layout(tree, {10, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({1, 0}).cp == U'x');
    CHECK(grid.at({2, 0}).cp == U']');
}

TEST_CASE("render: radio button draws ( ) / (*)") {
    {
        auto [doc, tree] = make_tree(R"(<body><input type="radio"></body>)");
        const Box box = layout(tree, {10, 3});
        const CharGrid grid = tvshow::render::render(box);
        CHECK(grid.at({0, 0}).cp == U'(');
        CHECK(grid.at({1, 0}).cp == U' ');
        CHECK(grid.at({2, 0}).cp == U')');
    }
    {
        auto [doc, tree] = make_tree(R"(<body><input type="radio" checked></body>)");
        const Box box = layout(tree, {10, 3});
        const CharGrid grid = tvshow::render::render(box);
        CHECK(grid.at({1, 0}).cp == U'•');  // •
    }
}

TEST_CASE("render: submit button draws [ Label ]") {
    auto [doc, tree] = make_tree(R"(<body><input type="submit" value="Go"></body>)");
    const Box box = layout(tree, {20, 3});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({2, 0}).cp == U'G');
    CHECK(grid.at({3, 0}).cp == U'o');
    CHECK(grid.at({4, 0}).cp == U']');
}

TEST_CASE("render: hidden input is not rendered") {
    auto [doc, tree] = make_tree(R"(<body>x<input type="hidden" value="secret">y</body>)");
    const Box box = layout(tree, {20, 3});
    const CharGrid grid = tvshow::render::render(box);
    // hidden input occupies no space; 'x' and 'y' should be at cols 0 and 1
    CHECK(grid.at({0, 0}).cp == U'x');
    CHECK(grid.at({1, 0}).cp == U'y');
}

// ── list markers ─────────────────────────────────────────────────────────────

TEST_CASE("render: ul li gets bullet marker") {
    // ul has UA padding-left: 16px (2 cols); li content starts at col 2.
    // Marker is painted at col 2 - 2 = 0, same row as li content.
    auto [doc, tree] = make_tree("<body><ul><li>x</li></ul></body>");
    const Box box = layout(tree, {80, 24});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'•');
    CHECK(grid.at({2, 0}).cp == U'x');
}

TEST_CASE("render: ol lis get decimal markers") {
    auto [doc, tree] = make_tree("<body><ol><li>a</li><li>b</li></ol></body>");
    const Box box = layout(tree, {80, 24});
    const CharGrid grid = tvshow::render::render(box);
    CHECK(grid.at({0, 0}).cp == U'1');
    CHECK(grid.at({1, 0}).cp == U'.');
    CHECK(grid.at({2, 0}).cp == U'a');
    CHECK(grid.at({0, 1}).cp == U'2');
    CHECK(grid.at({1, 1}).cp == U'.');
    CHECK(grid.at({2, 1}).cp == U'b');
}

// ── table rendering (SPEC §6.6, ADR 002) ─────────────────────────────────────

TEST_CASE("render: table cells rendered as equal-width flex columns") {
    // Table with 2 cells on a 60-col viewport.
    // table border: 1 col each side → content width 58
    // each td: flex-grow 1, padding-left/right 8px (1ch each)
    // each td border_box ≈ 29 cols, content ≈ 27 cols
    auto [doc, tree] = make_tree(
        "<body><table><tr><td>AA</td><td>BB</td></tr></table></body>");
    const Box box = layout(tree, {60, 24});
    const CharGrid grid = tvshow::render::render(box);
    // table solid border: '┌' at top-left (0,0)
    CHECK(grid.at({0, 0}).cp == U'┌');
    // 'AA' inside first cell: col 2 (1 border + 1 padding), row 1 (1 border)
    CHECK(grid.at({2, 1}).cp == U'A');
    CHECK(grid.at({3, 1}).cp == U'A');
    // 'BB' inside second cell: starts at col 31 (1 border + 29 for td0 + 1 padding)
    CHECK(grid.at({31, 1}).cp == U'B');
    CHECK(grid.at({32, 1}).cp == U'B');
}

// ── apply_debug_overlay (SPEC §20 Q-12) ──────────────────────────────────────

TEST_CASE("render: apply_debug_overlay draws corners on a multi-row box") {
    // Build a synthetic box tree with one root box 10 wide × 3 tall.
    CharGrid grid(10, 3);
    const tvshow::render::ColorAttr plain{0xFFFFFFU, 0x000000U};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 10; ++c) {
            grid.put({c, r}, U' ', plain);
        }
    }

    Box root;
    root.border_box = {{0, 0}, {10, 3}};

    tvshow::render::apply_debug_overlay(grid, root);

    CHECK(grid.at({0, 0}).cp == U'┌');
    CHECK(grid.at({9, 0}).cp == U'┐');
    CHECK(grid.at({0, 2}).cp == U'└');
    CHECK(grid.at({9, 2}).cp == U'┘');
    CHECK(grid.at({1, 0}).cp == U'─');  // top edge
    CHECK(grid.at({0, 1}).cp == U'│');  // left edge
    CHECK(grid.at({0, 0}).attr.fg == 0xFF00FFU);  // magenta
}

TEST_CASE("render: apply_debug_overlay single-row box uses bracket markers") {
    CharGrid grid(5, 1);
    const tvshow::render::ColorAttr plain{0xFFFFFFU, 0x000000U};
    for (int c = 0; c < 5; ++c) { grid.put({c, 0}, U' ', plain); }

    Box root;
    root.border_box = {{0, 0}, {5, 1}};

    tvshow::render::apply_debug_overlay(grid, root);

    CHECK(grid.at({0, 0}).cp == U'[');
    CHECK(grid.at({4, 0}).cp == U']');
}
