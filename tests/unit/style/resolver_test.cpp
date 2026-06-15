#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using tvshow::css::Stylesheet;
using tvshow::dom::Document;
using tvshow::style::Display;
using tvshow::style::FontStyle;
using tvshow::style::FontWeight;
using tvshow::style::LengthUnit;
using tvshow::style::parse_color;
using tvshow::style::parse_length;
using tvshow::style::resolve;
using tvshow::style::StyledNode;
using tvshow::style::TextDecoration;

// ── helpers ──────────────────────────────────────────────────────────────────

static Document make_doc(std::string_view html) {
    auto doc = tvshow::dom::parse(html);
    REQUIRE(doc.has_value());
    return std::move(*doc);
}

// NOLINTNEXTLINE(misc-no-recursion)
static const StyledNode* find_tag(const StyledNode& root, std::string_view tag) {
    if (root.node != nullptr && root.node->tag == tag) {
        return &root;
    }
    for (const auto& child : root.children) {
        const auto* found = find_tag(child, tag);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

static std::span<const Stylesheet> no_sheets() {
    return {};
}

// ── parse_color ───────────────────────────────────────────────────────────────

TEST_CASE("style::parse_color: known named colors") {
    const auto red = parse_color("red");
    CHECK_FALSE(red.none);
    CHECK(red.r == 255);
    CHECK(red.g == 0);
    CHECK(red.b == 0);

    const auto white = parse_color("white");
    CHECK_FALSE(white.none);
    CHECK(white.r == 255);
    CHECK(white.g == 255);
    CHECK(white.b == 255);
}

TEST_CASE("style::parse_color: #rrggbb") {
    const auto c = parse_color("#ff8000");
    CHECK_FALSE(c.none);
    CHECK(c.r == 0xFF);
    CHECK(c.g == 0x80);
    CHECK(c.b == 0x00);
}

TEST_CASE("style::parse_color: #rgb shorthand") {
    const auto c = parse_color("#f80");
    CHECK_FALSE(c.none);
    CHECK(c.r == 0xFF);
    CHECK(c.g == 0x88);
    CHECK(c.b == 0x00);
}

TEST_CASE("style::parse_color: unknown returns none") {
    const auto c = parse_color("chartreuse-plaid");
    CHECK(c.none);
}

// ── parse_length ──────────────────────────────────────────────────────────────

TEST_CASE("style::parse_length: px") {
    const auto l = parse_length("16px");
    CHECK_FALSE(l.is_auto);
    CHECK(l.unit == LengthUnit::Px);
    CHECK(l.value == doctest::Approx(static_cast<double>(16.0F)));
}

TEST_CASE("style::parse_length: percent") {
    const auto l = parse_length("50%");
    CHECK_FALSE(l.is_auto);
    CHECK(l.unit == LengthUnit::Pct);
    CHECK(l.value == doctest::Approx(static_cast<double>(50.0F)));
}

TEST_CASE("style::parse_length: ch") {
    const auto l = parse_length("20ch");
    CHECK_FALSE(l.is_auto);
    CHECK(l.unit == LengthUnit::Ch);
    CHECK(l.value == doctest::Approx(static_cast<double>(20.0F)));
}

TEST_CASE("style::parse_length: auto") {
    const auto l = parse_length("auto");
    CHECK(l.is_auto);
}

TEST_CASE("style::parse_length: zero without unit") {
    const auto l = parse_length("0");
    CHECK_FALSE(l.is_auto);
    CHECK(l.value == doctest::Approx(static_cast<double>(0.0F)));
}

// ── UA defaults ───────────────────────────────────────────────────────────────

TEST_CASE("style::resolve: body is display:block by default") {
    const auto doc = make_doc("<body></body>");
    const auto tree = resolve(doc, no_sheets());
    REQUIRE(tree.has_value());
    const auto* body = find_tag(*tree, "body");
    REQUIRE(body != nullptr);
    CHECK(body->style.display == Display::Block);
}

TEST_CASE("style::resolve: p is display:block by default") {
    const auto doc = make_doc("<body><p>text</p></body>");
    const auto tree = resolve(doc, no_sheets());
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.display == Display::Block);
}

TEST_CASE("style::resolve: b has font-weight:bold by default") {
    const auto doc = make_doc("<body><b>bold</b></body>");
    const auto tree = resolve(doc, no_sheets());
    REQUIRE(tree.has_value());
    const auto* b = find_tag(*tree, "b");
    REQUIRE(b != nullptr);
    CHECK(b->style.font_weight == FontWeight::Bold);
}

TEST_CASE("style::resolve: a has text-decoration:underline by default") {
    const auto doc = make_doc("<body><a href='#'>link</a></body>");
    const auto tree = resolve(doc, no_sheets());
    REQUIRE(tree.has_value());
    const auto* a = find_tag(*tree, "a");
    REQUIRE(a != nullptr);
    CHECK(a->style.text_decoration == TextDecoration::Underline);
}

TEST_CASE("style::resolve: em has font-style:italic by default") {
    const auto doc = make_doc("<body><em>text</em></body>");
    const auto tree = resolve(doc, no_sheets());
    REQUIRE(tree.has_value());
    const auto* em = find_tag(*tree, "em");
    REQUIRE(em != nullptr);
    CHECK(em->style.font_style == FontStyle::Italic);
}

// ── Author stylesheet ─────────────────────────────────────────────────────────

TEST_CASE("style::resolve: author tag rule sets color") {
    const auto doc = make_doc("<body><p>text</p></body>");
    const auto sheet = tvshow::css::parse("p { color: red; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK_FALSE(p->style.color.none);
    CHECK(p->style.color.r == 255);
    CHECK(p->style.color.g == 0);
    CHECK(p->style.color.b == 0);
}

TEST_CASE("style::resolve: class selector matches") {
    const auto doc = make_doc("<body><p class='hero'>text</p></body>");
    const auto sheet = tvshow::css::parse(".hero { font-weight: bold; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.font_weight == FontWeight::Bold);
}

TEST_CASE("style::resolve: id selector matches") {
    const auto doc = make_doc("<body><div id='main'>text</div></body>");
    const auto sheet = tvshow::css::parse("#main { display: none; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* div = find_tag(*tree, "div");
    REQUIRE(div != nullptr);
    CHECK(div->style.display == Display::None);
}

TEST_CASE("style::resolve: descendant selector matches nested element") {
    const auto doc = make_doc("<body><div><p>text</p></div></body>");
    const auto sheet = tvshow::css::parse("div p { color: red; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK_FALSE(p->style.color.none);
}

TEST_CASE("style::resolve: descendant selector does not match non-nested") {
    const auto doc = make_doc("<body><p>outside</p><div></div></body>");
    const auto sheet = tvshow::css::parse("div p { color: red; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.color.none);
}

TEST_CASE("style::resolve: specificity — id beats class") {
    const auto doc = make_doc("<body><p id='x' class='x'>text</p></body>");
    const auto sheet = tvshow::css::parse(".x { color: blue; } #x { color: red; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.color.r == 255);  // red from #x wins
    CHECK(p->style.color.b == 0);
}

TEST_CASE("style::resolve: !important overrides higher specificity") {
    const auto doc = make_doc("<body><p id='x'>text</p></body>");
    const auto sheet = tvshow::css::parse("#x { color: red; } p { color: blue !important; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.color.b == 255);  // blue from !important wins
    CHECK(p->style.color.r == 0);
}

TEST_CASE("style::resolve: inline style overrides author stylesheet") {
    const auto doc = make_doc("<body><p style='color: red;'>text</p></body>");
    const auto sheet = tvshow::css::parse("p { color: blue; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK(p->style.color.r == 255);  // red from inline wins
    CHECK(p->style.color.b == 0);
}

TEST_CASE("style::resolve: color inherits from parent") {
    const auto doc = make_doc("<body><p>text</p></body>");
    const auto sheet = tvshow::css::parse("body { color: red; }");
    REQUIRE(sheet.has_value());
    const std::vector<Stylesheet> sheets{*sheet};
    const auto tree = resolve(doc, sheets);
    REQUIRE(tree.has_value());
    const auto* p = find_tag(*tree, "p");
    REQUIRE(p != nullptr);
    CHECK_FALSE(p->style.color.none);
    CHECK(p->style.color.r == 255);
}
