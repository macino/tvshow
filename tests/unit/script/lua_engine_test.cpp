#include "tvshow/script/lua_engine.hpp"

#include "tvshow/dom/parser.hpp"

#include <doctest/doctest.h>

#include <string>
#include <utility>

using tvshow::script::LuaSession;

namespace {
tvshow::dom::Document parse_or_fail(std::string_view html) {
    auto doc = tvshow::dom::parse(html);
    REQUIRE(doc.has_value());
    return std::move(*doc);
}
}  // namespace

TEST_CASE("LuaSession: set_text mutates the target node") {
    auto doc = parse_or_fail("<html><body><span id=\"display\">0</span></body></html>");
    LuaSession session(doc, "function press() tv.set_text('display', '56') end");
    REQUIRE(session.ok());
    const auto outcome = session.call("press");
    CHECK(outcome.ok);
    CHECK(doc.body()->children[0]->children[0]->text == "56");
}

TEST_CASE("LuaSession: set_text only touches the matching id") {
    auto doc = parse_or_fail(
        "<html><body><span id=\"a\">a0</span><span id=\"b\">b0</span></body></html>");
    LuaSession session(doc, "function press() tv.set_text('a', 'a1') end");
    REQUIRE(session.ok());
    CHECK(session.call("press").ok);
    const auto* body = doc.body();
    CHECK(body->children[0]->children[0]->text == "a1");
    CHECK(body->children[1]->children[0]->text == "b0");
}

TEST_CASE("LuaSession: get_text reads current content") {
    auto doc = parse_or_fail("<html><body><span id=\"x\">hi</span></body></html>");
    LuaSession session(doc,
                       "result = nil\n"
                       "function press() result = tv.get_text('x') end");
    REQUIRE(session.ok());
    CHECK(session.call("press").ok);
}

TEST_CASE("LuaSession: state persists across calls (closures over top-level locals)") {
    auto doc = parse_or_fail("<html><body><span id=\"total\">0</span></body></html>");
    LuaSession session(doc,
                       "local n = 0\n"
                       "function bump() n = n + 1 tv.set_text('total', tostring(n)) end");
    REQUIRE(session.ok());
    CHECK(session.call("bump").ok);
    CHECK(session.call("bump").ok);
    CHECK(session.call("bump").ok);
    CHECK(doc.body()->children[0]->children[0]->text == "3");
}

TEST_CASE("LuaSession: unknown id is a no-op, not a crash") {
    auto doc = parse_or_fail("<html><body></body></html>");
    LuaSession session(doc, "function press() tv.set_text('nope', 'x') end");
    REQUIRE(session.ok());
    CHECK(session.call("press").ok);
}

TEST_CASE("LuaSession: calling an undefined handler fails gracefully") {
    auto doc = parse_or_fail("<html><body></body></html>");
    LuaSession session(doc, "function press() end");
    REQUIRE(session.ok());
    const auto outcome = session.call("no_such_function");
    CHECK_FALSE(outcome.ok);
    CHECK_FALSE(outcome.error.empty());
}

TEST_CASE("LuaSession: syntax error at load time fails without crashing") {
    auto doc = parse_or_fail("<html><body></body></html>");
    LuaSession session(doc, "function press( end");
    CHECK_FALSE(session.ok());
    CHECK_FALSE(session.error().empty());
}

TEST_CASE("LuaSession: runtime error in the handler is caught, not fatal") {
    auto doc = parse_or_fail("<html><body></body></html>");
    LuaSession session(doc, "function press() error('boom') end");
    REQUIRE(session.ok());
    const auto outcome = session.call("press");
    CHECK_FALSE(outcome.ok);
    CHECK(outcome.error.find("boom") != std::string::npos);
}

TEST_CASE("LuaSession: infinite loop is bounded by the instruction budget") {
    auto doc = parse_or_fail("<html><body></body></html>");
    LuaSession session(doc, "function press() while true do end end");
    REQUIRE(session.ok());
    const auto outcome = session.call("press");
    CHECK_FALSE(outcome.ok);
}

TEST_CASE("LuaSession: io/os/load/require are unavailable (sandboxing regression test)") {
    auto doc = parse_or_fail("<html><body><span id=\"r\">?</span></body></html>");
    LuaSession session(doc,
                       "function check() "
                       "  local missing = (io == nil) and (os == nil) and (load == nil) and "
                       "                  (require == nil) and (dofile == nil) and "
                       "                  (loadstring == nil)"
                       "  tv.set_text('r', missing and 'sandboxed' or 'leaky') "
                       "end");
    REQUIRE(session.ok());
    CHECK(session.call("check").ok);
    CHECK(doc.body()->children[0]->children[0]->text == "sandboxed");
}
