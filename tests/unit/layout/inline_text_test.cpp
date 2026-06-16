#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/inline_text.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using tvshow::css::Stylesheet;
using tvshow::layout::break_inline;
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
    std::vector<Stylesheet> sheets;
    if (!css.empty()) {
        auto sheet = tvshow::css::parse(css);
        if (sheet.has_value()) {
            sheets.push_back(*sheet);
        }
    }
    auto tree = tvshow::style::resolve(*doc, sheets);
    REQUIRE(tree.has_value());
    return {std::move(*doc), std::move(*tree)};
}

// NOLINTNEXTLINE(misc-no-recursion)
const StyledNode* find_tag(const StyledNode& root, std::string_view tag) {
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

std::string line_text(const tvshow::layout::InlineLine& line) {
    std::string s;
    for (const auto& tok : line) {
        s += static_cast<char>(tok.cp);
    }
    return s;
}

}  // namespace

TEST_CASE("break_inline: word wrap breaks at word boundaries, not mid-word") {
    auto [doc, tree] = make_tree("<body><p>ab cd ef</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    // "ab cd ef" is 8 chars; width 5 fits "ab cd" (5) but not "ab cd ef" (8).
    const auto lines = break_inline(*p, 5);
    REQUIRE(lines.size() == 2);
    CHECK(line_text(lines[0]) == "ab cd");
    CHECK(line_text(lines[1]) == "ef");
}

TEST_CASE("break_inline: a single word longer than the line hard-breaks by character") {
    auto [doc, tree] = make_tree("<body><p>abcdefghij</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 5);
    REQUIRE(lines.size() == 2);
    CHECK(line_text(lines[0]) == "abcde");
    CHECK(line_text(lines[1]) == "fghij");
}

TEST_CASE("break_inline: normal collapses runs of whitespace into a single space") {
    auto [doc, tree] = make_tree("<body><p>a    b</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 20);
    REQUIRE(lines.size() == 1);
    CHECK(line_text(lines[0]) == "a b");
}

TEST_CASE("break_inline: white-space:pre preserves spaces and breaks only on newline") {
    auto [doc, tree] = make_tree("<body><p>a    b\nc</p></body>", "p { white-space: pre; }");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 3);
    REQUIRE(lines.size() == 2);
    CHECK(line_text(lines[0]) == "a    b");  // overflows width 3 — pre never wraps
    CHECK(line_text(lines[1]) == "c");
}

TEST_CASE("break_inline: white-space:nowrap collapses whitespace but never wraps") {
    auto [doc, tree] = make_tree("<body><p>ab cd ef</p></body>", "p { white-space: nowrap; }");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 5);
    REQUIRE(lines.size() == 1);
    CHECK(line_text(lines[0]) == "ab cd ef");
}

TEST_CASE("break_inline: inline descendants contribute their text to the same line stream") {
    auto [doc, tree] = make_tree("<body><p>a <b>bold</b> c</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 20);
    REQUIRE(lines.size() == 1);
    CHECK(line_text(lines[0]) == "a bold c");
}

TEST_CASE("break_inline: zero content width yields no lines") {
    auto [doc, tree] = make_tree("<body><p>text</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    CHECK(break_inline(*p, 0).empty());
}

TEST_CASE("break_inline: tokens inside <a href> carry the link target") {
    auto [doc, tree] = make_tree("<body><p>see <a href=\"/x\">this</a> page</p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    const auto lines = break_inline(*p, 20);
    REQUIRE(lines.size() == 1);
    // "see this page" — only the 4 chars of "this" carry the href.
    const auto& line = lines[0];
    REQUIRE(line.size() == std::string_view("see this page").size());
    for (size_t i = 0; i < line.size(); ++i) {
        const bool in_link = i >= 4 && i < 8;  // "this" spans [4, 8)
        CHECK(line[i].href.empty() != in_link);
    }
}

TEST_CASE("break_inline: no inline content yields no lines") {
    auto [doc, tree] = make_tree("<body><p></p></body>");
    const auto* p = find_tag(tree, "p");
    REQUIRE(p != nullptr);
    CHECK(break_inline(*p, 10).empty());
}
