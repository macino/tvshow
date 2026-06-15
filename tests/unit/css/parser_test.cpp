#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"

#include <doctest/doctest.h>

#include <optional>

using tvshow::css::Combinator;
using tvshow::css::ComplexSel;
using tvshow::css::parse;
using tvshow::css::parse_inline;
using tvshow::css::Rule;
using tvshow::css::SimpleSel;

TEST_CASE("css::parse: empty stylesheet") {
    const auto ss = parse("");
    REQUIRE(ss.has_value());
    CHECK(ss->rules.empty());
}

TEST_CASE("css::parse: single rule with one declaration") {
    const auto ss = parse("p { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const Rule& rule = ss->rules[0];
    REQUIRE(rule.selectors.size() == 1);
    REQUIRE(rule.declarations.size() == 1);
    CHECK(rule.declarations[0].property == "color");
    CHECK(rule.declarations[0].value == "red");
    CHECK_FALSE(rule.declarations[0].important);
}

TEST_CASE("css::parse: important flag") {
    const auto ss = parse("p { color: red !important; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    CHECK(ss->rules[0].declarations[0].important);
}

TEST_CASE("css::parse: multiple declarations") {
    const auto ss = parse("div { color: blue; background-color: white; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    CHECK(ss->rules[0].declarations.size() == 2);
    CHECK(ss->rules[0].declarations[0].property == "color");
    CHECK(ss->rules[0].declarations[1].property == "background-color");
}

TEST_CASE("css::parse: multiple rules") {
    const auto ss = parse("p { color: red; } div { color: blue; }");
    REQUIRE(ss.has_value());
    CHECK(ss->rules.size() == 2);
}

TEST_CASE("css::parse: tag selector") {
    const auto ss = parse("div { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    REQUIRE(ss->rules[0].selectors.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(sel.compounds.size() == 1);
    REQUIRE(sel.compounds[0].simples.size() == 1);
    CHECK(sel.compounds[0].simples[0].kind == SimpleSel::Kind::Tag);
    CHECK(sel.compounds[0].simples[0].value == "div");
}

TEST_CASE("css::parse: class selector") {
    const auto ss = parse(".foo { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(sel.compounds.size() == 1);
    REQUIRE(!sel.compounds[0].simples.empty());
    CHECK(sel.compounds[0].simples[0].kind == SimpleSel::Kind::Class);
    CHECK(sel.compounds[0].simples[0].value == "foo");
}

TEST_CASE("css::parse: id selector") {
    const auto ss = parse("#header { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(!sel.compounds[0].simples.empty());
    CHECK(sel.compounds[0].simples[0].kind == SimpleSel::Kind::Id);
    CHECK(sel.compounds[0].simples[0].value == "header");
}

TEST_CASE("css::parse: compound selector div.foo") {
    const auto ss = parse("div.foo { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(sel.compounds.size() == 1);
    CHECK(sel.compounds[0].simples.size() == 2);
}

TEST_CASE("css::parse: descendant combinator") {
    const auto ss = parse("div p { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(sel.compounds.size() == 2);
    REQUIRE(sel.combinators.size() == 1);
    CHECK(sel.combinators[0] == Combinator::Descendant);
    CHECK(sel.compounds[0].simples[0].value == "div");
    CHECK(sel.compounds[1].simples[0].value == "p");
}

TEST_CASE("css::parse: child combinator") {
    const auto ss = parse("ul > li { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    const ComplexSel& sel = ss->rules[0].selectors[0];
    REQUIRE(sel.compounds.size() == 2);
    REQUIRE(sel.combinators.size() == 1);
    CHECK(sel.combinators[0] == Combinator::Child);
}

TEST_CASE("css::parse: comma-separated selectors") {
    const auto ss = parse("h1, h2, h3 { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 1);
    CHECK(ss->rules[0].selectors.size() == 3);
}

TEST_CASE("css::parse: specificity: id > class > tag") {
    const auto ss = parse("#id { color: red; } .cls { color: red; } p { color: red; }");
    REQUIRE(ss.has_value());
    REQUIRE(ss->rules.size() == 3);
    const int id_spec = ss->rules[0].selectors[0].specificity;
    const int cls_spec = ss->rules[1].selectors[0].specificity;
    const int tag_spec = ss->rules[2].selectors[0].specificity;
    CHECK(id_spec > cls_spec);
    CHECK(cls_spec > tag_spec);
    CHECK(tag_spec > 0);
}

TEST_CASE("css::parse: @media rule ignored gracefully") {
    const auto ss = parse("@media screen { p { color: red; } } div { color: blue; }");
    REQUIRE(ss.has_value());
    // @media rules are not in scope — only top-level style rules extracted.
    // div rule must still be present.
    bool has_div = false;
    for (const auto& r : ss->rules) {
        for (const auto& sel : r.selectors) {
            if (!sel.compounds.empty() && !sel.compounds[0].simples.empty() &&
                sel.compounds[0].simples[0].value == "div")
                has_div = true;
        }
    }
    CHECK(has_div);
}

TEST_CASE("css::parse_inline: empty") {
    const auto decls = parse_inline("");
    CHECK(decls.empty());
}

TEST_CASE("css::parse_inline: single property") {
    const auto decls = parse_inline("color: red");
    REQUIRE(decls.size() == 1);
    CHECK(decls[0].property == "color");
    CHECK(decls[0].value == "red");
}

TEST_CASE("css::parse_inline: multiple properties") {
    const auto decls = parse_inline("color: red; font-weight: bold");
    REQUIRE(decls.size() == 2);
    CHECK(decls[0].property == "color");
    CHECK(decls[1].property == "font-weight");
}
