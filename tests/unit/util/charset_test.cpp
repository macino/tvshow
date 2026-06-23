#include "tvshow/util/charset.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using tvshow::util::prescan_charset;
using tvshow::util::transcode_to_utf8;

// ── transcode_to_utf8 ─────────────────────────────────────────────────────────

TEST_CASE("util::charset: transcode_to_utf8 is a no-op for UTF-8 charset") {
    const std::string hello = "hello";
    CHECK(transcode_to_utf8(hello, "utf-8") == hello);
    CHECK(transcode_to_utf8(hello, "UTF-8") == hello);
    CHECK(transcode_to_utf8(hello, "utf8") == hello);
    CHECK(transcode_to_utf8(hello, "") == hello);
    CHECK(transcode_to_utf8(hello, "us-ascii") == hello);
}

TEST_CASE("util::charset: transcode_to_utf8 converts ISO-8859-1 to UTF-8") {
    // \xF6 = 'ö' in ISO-8859-1 → U+00F6 → UTF-8: \xC3\xB6
    const std::string latin1 = "Sch\xF6n";
    const std::string utf8 = transcode_to_utf8(latin1, "ISO-8859-1");
    CHECK(utf8 == "Sch\xC3\xB6n");
}

TEST_CASE("util::charset: transcode_to_utf8 returns src unchanged for unknown charset") {
    const std::string src = "hello";
    CHECK(transcode_to_utf8(src, "X-NONEXISTENT-CHARSET-XYZ") == src);
}

// ── prescan_charset ───────────────────────────────────────────────────────────

TEST_CASE("util::charset: prescan_charset finds meta charset= attribute") {
    const auto cs = prescan_charset(R"(<html><head><meta charset="ISO-8859-1"></head>)");
    CHECK(cs == "ISO-8859-1");
}

TEST_CASE("util::charset: prescan_charset finds charset in http-equiv meta") {
    const auto cs = prescan_charset(
        R"(<meta http-equiv="Content-Type" content="text/html; charset=windows-1252">)");
    CHECK(cs == "windows-1252");
}

TEST_CASE("util::charset: prescan_charset is case-insensitive in search") {
    const auto cs = prescan_charset(R"(<meta CHARSET='utf-8'>)");
    CHECK(cs == "utf-8");
}

TEST_CASE("util::charset: prescan_charset returns empty when no charset found") {
    const auto cs = prescan_charset("<html><body>no charset here</body></html>");
    CHECK(cs.empty());
}

TEST_CASE("util::charset: prescan_charset respects the 2048-byte scan limit") {
    // Pad with 2048 spaces then add a charset — should NOT be found.
    const std::string padded(2048, ' ');
    const auto cs = prescan_charset(padded + R"(<meta charset="ISO-8859-1">)");
    CHECK(cs.empty());
}
