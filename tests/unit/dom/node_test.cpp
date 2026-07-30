#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"

#include <doctest/doctest.h>

using tvshow::dom::collect_lua_scripts;
using tvshow::dom::find_by_id;
using tvshow::dom::find_meta_content;
using tvshow::dom::parse;

TEST_CASE("find_by_id: finds a matching element anywhere in the tree") {
    const auto doc = parse("<html><body><div><span id=\"display\">0</span></div></body></html>");
    REQUIRE(doc.has_value());
    const auto* found = find_by_id(*doc->root, "display");
    REQUIRE(found != nullptr);
    CHECK(found->tag == "span");
}

TEST_CASE("find_by_id: no match returns nullptr") {
    const auto doc = parse("<html><body><span id=\"a\">x</span></body></html>");
    REQUIRE(doc.has_value());
    CHECK(find_by_id(*doc->root, "nope") == nullptr);
}

TEST_CASE("find_by_id: mutable overload lets the caller write through") {
    auto doc = parse("<html><body><span id=\"display\">0</span></body></html>");
    REQUIRE(doc.has_value());
    auto* node = find_by_id(*doc->root, "display");
    REQUIRE(node != nullptr);
    REQUIRE(node->children.size() == 1);
    node->children[0]->text = "56";
    const auto* reread = find_by_id(*doc->root, "display");
    REQUIRE(reread != nullptr);
    CHECK(reread->children[0]->text == "56");
}

TEST_CASE("collect_lua_scripts: concatenates text/lua script bodies") {
    const auto doc = parse(
        "<html><body><script type=\"text/lua\">function a() end</script>"
        "<script type=\"text/lua\">function b() end</script></body></html>");
    REQUIRE(doc.has_value());
    const auto src = collect_lua_scripts(*doc);
    CHECK(src.find("function a() end") != std::string::npos);
    CHECK(src.find("function b() end") != std::string::npos);
}

TEST_CASE("collect_lua_scripts: ignores scripts without type=text/lua") {
    const auto doc =
        parse("<html><body><script>var x = 1;</script></body></html>");
    REQUIRE(doc.has_value());
    CHECK(collect_lua_scripts(*doc).find("var x") == std::string::npos);
}

TEST_CASE("collect_lua_scripts: empty when no script tags") {
    const auto doc = parse("<html><body><p>hi</p></body></html>");
    REQUIRE(doc.has_value());
    CHECK(collect_lua_scripts(*doc).empty());
}

TEST_CASE("find_meta_content: returns content of matching meta name") {
    const auto doc = parse(
        "<html><head><meta name=\"tvshow-window-size\" content=\"26x14\"></head>"
        "<body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(find_meta_content(*doc, "tvshow-window-size") == "26x14");
}

TEST_CASE("find_meta_content: empty when no matching meta") {
    const auto doc = parse("<html><head></head><body></body></html>");
    REQUIRE(doc.has_value());
    CHECK(find_meta_content(*doc, "tvshow-window-size").empty());
}
