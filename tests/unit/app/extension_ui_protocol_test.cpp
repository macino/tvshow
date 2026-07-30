#include "tvshow/app/extension_ui_protocol.hpp"

#include <doctest/doctest.h>

using namespace tvshow::app;

TEST_CASE("parse_ui_command: UI_INIT") {
    CHECK(std::holds_alternative<UiInit>(parse_ui_command("UI_INIT")));
}

TEST_CASE("parse_ui_command: CLEAR") {
    CHECK(std::holds_alternative<UiClear>(parse_ui_command("CLEAR")));
}

TEST_CASE("parse_ui_command: BUTTON with all fields") {
    const auto cmd = parse_ui_command(R"(BUTTON id=1 x=2 y=3 w=5 label="7")");
    REQUIRE(std::holds_alternative<UiButton>(cmd));
    const auto& b = std::get<UiButton>(cmd);
    CHECK(b.id == 1);
    CHECK(b.x == 2);
    CHECK(b.y == 3);
    CHECK(b.w == 5);
    CHECK(b.label == "7");
}

TEST_CASE("parse_ui_command: BUTTON missing a required field is ignored") {
    const auto cmd = parse_ui_command(R"(BUTTON id=1 x=2 y=3 label="7")");  // no w=
    CHECK(std::holds_alternative<UiIgnored>(cmd));
}

TEST_CASE("parse_ui_command: TEXT defaults h to 1 when absent") {
    const auto cmd = parse_ui_command(R"(TEXT id=1 x=0 y=0 w=10 value="hi")");
    REQUIRE(std::holds_alternative<UiText>(cmd));
    const auto& t = std::get<UiText>(cmd);
    CHECK(t.h == 1);
    CHECK(t.value == "hi");
}

TEST_CASE("parse_ui_command: TEXT with explicit h") {
    const auto cmd = parse_ui_command(R"(TEXT id=1 x=0 y=0 w=10 h=3 value="hi")");
    REQUIRE(std::holds_alternative<UiText>(cmd));
    CHECK(std::get<UiText>(cmd).h == 3);
}

TEST_CASE("parse_ui_command: TEXT value unescapes \\n to a real newline") {
    const auto cmd = parse_ui_command(R"(TEXT id=1 x=0 y=0 w=10 value="line1\nline2")");
    REQUIRE(std::holds_alternative<UiText>(cmd));
    CHECK(std::get<UiText>(cmd).value == "line1\nline2");
}

TEST_CASE("parse_ui_command: TEXT value unescapes \\\" to a literal quote") {
    const auto cmd = parse_ui_command(R"(TEXT id=1 x=0 y=0 w=10 value="say \"hi\"")");
    REQUIRE(std::holds_alternative<UiText>(cmd));
    CHECK(std::get<UiText>(cmd).value == "say \"hi\"");
}

TEST_CASE("parse_ui_command: TITLE") {
    const auto cmd = parse_ui_command(R"(TITLE value="Calculator")");
    REQUIRE(std::holds_alternative<UiTitle>(cmd));
    CHECK(std::get<UiTitle>(cmd).value == "Calculator");
}

TEST_CASE("parse_ui_command: unknown command word is ignored") {
    CHECK(std::holds_alternative<UiIgnored>(parse_ui_command("FROBNICATE x=1")));
}

TEST_CASE("parse_ui_command: empty line is ignored") {
    CHECK(std::holds_alternative<UiIgnored>(parse_ui_command("")));
    CHECK(std::holds_alternative<UiIgnored>(parse_ui_command("   ")));
}

TEST_CASE("parse_ui_command: unterminated quoted value is ignored, not a crash") {
    const auto cmd = parse_ui_command(R"(TEXT id=1 x=0 y=0 w=10 value="unterminated)");
    CHECK(std::holds_alternative<UiIgnored>(cmd));
}

TEST_CASE("parse_ui_command: non-numeric id field is ignored") {
    const auto cmd = parse_ui_command(R"(BUTTON id=abc x=2 y=3 w=5 label="7")");
    CHECK(std::holds_alternative<UiIgnored>(cmd));
}

TEST_CASE("format_click_event") {
    CHECK(format_click_event(7) == "CLICK id=7");
    CHECK(format_click_event(0) == "CLICK id=0");
}
