#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <optional>
#include <string>

using tvshow::dom::Document;
using tvshow::dom::Node;
using tvshow::dom::NodeKind;
using tvshow::dom::parse;

TEST_CASE("dom::parse: minimal document") {
    const auto doc = parse("<html><head></head><body></body></html>");
    REQUIRE(doc.has_value());
    REQUIRE(doc->root != nullptr);
    CHECK(doc->root->tag == "html");
}

TEST_CASE("dom::parse: title extracted") {
    const auto doc = parse("<html><head><title>Hello World</title></head><body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(doc->title == "Hello World");
}

TEST_CASE("dom::parse: body() convenience accessor") {
    const auto doc = parse("<html><head></head><body><p>hi</p></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    CHECK(b->tag == "body");
}

TEST_CASE("dom::parse: body() returns null for empty root") {
    Document empty_doc;
    CHECK(empty_doc.body() == nullptr);
}

TEST_CASE("dom::parse: charset from meta charset attribute") {
    const auto doc = parse("<html><head><meta charset=\"iso-8859-1\"></head><body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(doc->charset == "iso-8859-1");
}

TEST_CASE("dom::parse: charset defaults to utf-8") {
    const auto doc = parse("<html><head></head><body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(doc->charset == "utf-8");
}

TEST_CASE("dom::parse: charset hint from caller overrides default") {
    const auto doc = parse("<html><head></head><body></body></html>", "windows-1252");
    REQUIRE(doc.has_value());
    CHECK(doc->charset == "windows-1252");
}

TEST_CASE("dom::parse: stylesheet href collected") {
    const auto doc = parse("<html><head>"
                           "<link rel=\"stylesheet\" href=\"style.css\">"
                           "</head><body></body></html>");
    REQUIRE(doc.has_value());
    REQUIRE(doc->stylesheet_hrefs.size() == 1);
    CHECK(doc->stylesheet_hrefs[0] == "style.css");
}

TEST_CASE("dom::parse: multiple stylesheet hrefs") {
    const auto doc = parse("<html><head>"
                           "<link rel=\"stylesheet\" href=\"a.css\">"
                           "<link rel=\"stylesheet\" href=\"b.css\">"
                           "</head><body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(doc->stylesheet_hrefs.size() == 2);
}

TEST_CASE("dom::parse: inline style collected") {
    const auto doc = parse("<html><head>"
                           "<style>body { color: red; }</style>"
                           "</head><body></body></html>");
    REQUIRE(doc.has_value());
    REQUIRE(doc->inline_styles.size() == 1);
    CHECK(doc->inline_styles[0].find("color") != std::string::npos);
}

TEST_CASE("dom::parse: element children") {
    const auto doc = parse("<html><head></head><body><div><p></p></div></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    REQUIRE(!b->children.empty());
    CHECK(b->children[0]->tag == "div");
}

TEST_CASE("dom::parse: text node") {
    const auto doc = parse("<html><head></head><body><p>hello</p></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    REQUIRE(!b->children.empty());
    const Node* p = b->children[0].get();
    REQUIRE(p != nullptr);
    REQUIRE(!p->children.empty());
    CHECK(p->children[0]->kind == NodeKind::Text);
    CHECK(p->children[0]->text == "hello");
}

TEST_CASE("dom::parse: element attribute") {
    const auto doc =
        parse("<html><head></head><body><a href=\"http://x.com\">link</a></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    REQUIRE(!b->children.empty());
    const Node* a = b->children[0].get();
    REQUIRE(a != nullptr);
    CHECK(a->attr("href") == "http://x.com");
}

TEST_CASE("dom::parse: missing attribute returns empty") {
    const auto doc = parse("<html><head></head><body><div></div></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    CHECK(b->children[0]->attr("href").empty());
}

TEST_CASE("dom::parse: has_class positive") {
    const auto doc = parse("<html><head></head><body><div class=\"foo bar\"></div></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    CHECK(b->children[0]->has_class("foo"));
    CHECK(b->children[0]->has_class("bar"));
}

TEST_CASE("dom::parse: has_class negative") {
    const auto doc = parse("<html><head></head><body><div class=\"foo\"></div></body></html>");
    REQUIRE(doc.has_value());
    const Node* b = doc->body();
    REQUIRE(b != nullptr);
    CHECK_FALSE(b->children[0]->has_class("bar"));
}

TEST_CASE("dom::parse: lenient with missing html tags") {
    const auto doc = parse("<p>bare paragraph</p>");
    REQUIRE(doc.has_value());
    CHECK(doc->root != nullptr);
}

TEST_CASE("dom::parse: empty string") {
    const auto doc = parse("");
    REQUIRE(doc.has_value());
}
