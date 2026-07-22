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

TEST_CASE("layout: colspan cell width spans multiple columns") {
    auto [doc, tree] = make_tree(
        "<body><table>"
        "<tr><td colspan=\"2\">Wide</td></tr>"
        "<tr><td>Aaaaaaaaaa</td><td>Bbbbbbbbbb</td></tr>"
        "</table></body>");
    const Box root = layout(tree, {80, 24});
    const auto rows = find_all_boxes(root, "tr");
    REQUIRE(rows.size() >= 2);
    REQUIRE(!rows[0]->children.empty());
    REQUIRE(rows[1]->children.size() >= 2);

    // The colspan=2 cell should be as wide as both row-2 columns combined.
    const int spanned_w = rows[0]->children[0].border_box.size.cols;
    const int col0_w = rows[1]->children[0].border_box.size.cols;
    const int col1_w = rows[1]->children[1].border_box.size.cols;
    CHECK(spanned_w == col0_w + col1_w);
}

TEST_CASE("layout: colspan cell doesn't misalign the next row's columns") {
    // Without span-aware column indexing, row 2's second <td> would be
    // measured as if it were column index 1 instead of 2 (the colspan=2
    // cell in row 1 only "used up" one item, not two columns).
    auto [doc, tree] = make_tree(
        "<body><table>"
        "<tr><td colspan=\"2\">Wide</td><td>C</td></tr>"
        "<tr><td>A</td><td>B</td><td>Cccccccccc</td></tr>"
        "</table></body>");
    const Box root = layout(tree, {80, 24});
    const auto rows = find_all_boxes(root, "tr");
    REQUIRE(rows.size() >= 2);
    REQUIRE(rows[0]->children.size() >= 2);
    REQUIRE(rows[1]->children.size() >= 3);

    // Row 1's third cell ("C") should align under row 2's third column,
    // not its second -- i.e. it starts after the colspan=2 cell's full width.
    const int wide_w = rows[0]->children[0].border_box.size.cols;
    const int wide_start = rows[0]->children[0].border_box.origin.col;
    const int c_start = rows[0]->children[1].border_box.origin.col;
    CHECK(c_start == wide_start + wide_w);
}

TEST_CASE("layout: rowspan cell extends its box height across the spanned rows") {
    auto [doc, tree] = make_tree(
        "<body><table>"
        "<tr><td rowspan=\"2\">Tall</td><td>A</td></tr>"
        "<tr><td>B</td></tr>"
        "</table></body>");
    const Box root = layout(tree, {80, 24});
    const auto rows = find_all_boxes(root, "tr");
    REQUIRE(rows.size() >= 2);
    REQUIRE(!rows[0]->children.empty());

    const int tall_h = rows[0]->children[0].border_box.size.rows;
    const int row0_h = rows[0]->border_box.size.rows;
    const int row1_h = rows[1]->border_box.size.rows;
    // Spans its own row's height plus the next row's.
    CHECK(tall_h == row0_h + row1_h);
}

TEST_CASE("layout: rowspan cell doesn't misalign the following row's columns") {
    auto [doc, tree] = make_tree(
        "<body><table>"
        "<tr><td rowspan=\"2\">Tall</td><td>Aaaaaaaaaa</td></tr>"
        "<tr><td>Bbbbbbbbbb</td></tr>"
        "</table></body>");
    const Box root = layout(tree, {80, 24});
    const auto rows = find_all_boxes(root, "tr");
    REQUIRE(rows.size() >= 2);
    REQUIRE(!rows[0]->children.empty());
    REQUIRE(rows[1]->children.size() >= 1);

    // Row 2's single cell ("B") occupies the second grid column (skipping
    // the column still held by row 1's rowspan cell), so it should start
    // where row 1's second cell ("A") starts, not at column 0.
    const int row1_second_col_start = rows[0]->children[1].border_box.origin.col;
    const int row2_cell_start = rows[1]->children[0].border_box.origin.col;
    CHECK(row2_cell_start == row1_second_col_start);
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

TEST_CASE("layout: position:absolute skips a static intermediate to find the positioned ancestor") {
    auto [doc, tree] = make_tree(
        "<body><div style='position: relative; padding: 16px;'>"
        "<section><p>x</p></section></div></body>",
        "p { position: absolute; left: 8px; top: 16px; }");
    const Box root = layout(tree, {80, 24});
    const auto* div = find_box(root, "div");
    const auto* p = find_box(root, "p");
    REQUIRE(div != nullptr);
    REQUIRE(p != nullptr);
    // section is position:static (default) -- p must anchor to div, not section.
    CHECK(p->border_box.origin.col == div->content_box.origin.col + 1);
    CHECK(p->border_box.origin.row == div->content_box.origin.row + 1);
}

TEST_CASE("layout: position:absolute falls back to the viewport when no ancestor is positioned") {
    auto [doc, tree] = make_tree("<body><p>x</p></body>",
                                 "p { position: absolute; left: 8px; top: 16px; }");
    const Box root = layout(tree, {80, 24});
    const auto* p = find_box(root, "p");
    REQUIRE(p != nullptr);
    CHECK(p->border_box.origin.col == 1);  // 8px / 8 = 1 col from viewport origin
    CHECK(p->border_box.origin.row == 1);  // 16px / 16 = 1 row from viewport origin
}

TEST_CASE("layout: position:absolute is removed from flow and doesn't push down siblings") {
    auto [doc, tree] = make_tree(
        "<body><div style='height: 32px; position: absolute;'></div>"
        "<p>after</p></body>",
        "");
    const Box root = layout(tree, {80, 24});
    const auto* p = find_box(root, "p");
    REQUIRE(p != nullptr);
    // Had the absolute div reserved its 32px (2 rows) of flow space, p would
    // start at row 2 instead of row 0.
    CHECK(p->border_box.origin.row == 0);
}
