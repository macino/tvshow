#include "tvshow/render/chargrid.hpp"
#include "tvshow/types.hpp"

#include <doctest/doctest.h>

#include <stdexcept>

using tvshow::Point;
using tvshow::render::Cell;
using tvshow::render::CharGrid;
using tvshow::render::ColorAttr;

TEST_CASE("CharGrid: construction") {
    const CharGrid g(10, 3);
    CHECK(g.cols() == 10);
    CHECK(g.rows() == 3);
}

TEST_CASE("CharGrid: default cell is space with default colors") {
    const CharGrid g(5, 2);
    const Cell& c = g.at({0, 0});
    CHECK(c.cp == U' ');
    CHECK(c.attr.fg == 0xFFFFFFU);
    CHECK(c.attr.bg == 0x000000U);
    CHECK_FALSE(c.attr.bold);
    CHECK_FALSE(c.attr.italic);
    CHECK_FALSE(c.attr.underline);
    CHECK_FALSE(c.attr.strike);
}

TEST_CASE("CharGrid: put and at round-trip") {
    CharGrid g(5, 5);
    ColorAttr attr;
    attr.fg = 0xFF0000U;
    attr.bold = true;
    g.put({2, 1}, U'X', attr);
    const Cell& c = g.at({2, 1});
    CHECK(c.cp == U'X');
    CHECK(c.attr.fg == 0xFF0000U);
    CHECK(c.attr.bold);
}

TEST_CASE("CharGrid: other cells unchanged after put") {
    CharGrid g(3, 3);
    const ColorAttr attr;
    g.put({1, 1}, U'A', attr);
    CHECK(g.at({0, 0}).cp == U' ');
    CHECK(g.at({2, 2}).cp == U' ');
    CHECK(g.at({1, 1}).cp == U'A');
}

TEST_CASE("CharGrid: out-of-range put throws") {
    CharGrid g(4, 4);
    const ColorAttr attr;
    const Point left{-1, 0};
    const Point top{0, -1};
    const Point right{4, 0};
    const Point bottom{0, 4};
    CHECK_THROWS_AS(g.put(left, U'X', attr), std::out_of_range);
    CHECK_THROWS_AS(g.put(top, U'X', attr), std::out_of_range);
    CHECK_THROWS_AS(g.put(right, U'X', attr), std::out_of_range);
    CHECK_THROWS_AS(g.put(bottom, U'X', attr), std::out_of_range);
}

TEST_CASE("CharGrid: out-of-range at throws") {
    const CharGrid g(4, 4);
    const Point left{-1, 0};
    const Point right{4, 0};
    const Point bottom{0, 4};
    CHECK_THROWS_AS((void)g.at(left), std::out_of_range);
    CHECK_THROWS_AS((void)g.at(right), std::out_of_range);
    CHECK_THROWS_AS((void)g.at(bottom), std::out_of_range);
}

TEST_CASE("CharGrid: to_string / from_string round-trip (all-default)") {
    const CharGrid g(3, 2);
    auto s = g.to_string();
    auto g2 = CharGrid::from_string(s);
    REQUIRE(g2.has_value());
    CHECK(g2->cols() == 3);
    CHECK(g2->rows() == 2);
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 3; ++c) {
            CHECK(g2->at({c, r}) == g.at({c, r}));
        }
    }
}

TEST_CASE("CharGrid: to_string / from_string round-trip (styled cells)") {
    CharGrid g(4, 1);
    ColorAttr a;
    a.fg = 0x123456U;
    a.bg = 0xABCDEFU;
    a.bold = true;
    a.underline = true;
    g.put({0, 0}, U'H', a);
    g.put({1, 0}, U'i', ColorAttr{});
    g.put({2, 0}, U'!', ColorAttr{});
    g.put({3, 0}, U' ', ColorAttr{});

    auto s = g.to_string();
    auto g2 = CharGrid::from_string(s);
    REQUIRE(g2.has_value());
    CHECK(g2->at({0, 0}).cp == U'H');
    CHECK(g2->at({0, 0}).attr.fg == 0x123456U);
    CHECK(g2->at({0, 0}).attr.bg == 0xABCDEFU);
    CHECK(g2->at({0, 0}).attr.bold);
    CHECK(g2->at({0, 0}).attr.underline);
    CHECK_FALSE(g2->at({0, 0}).attr.italic);
    CHECK(g2->at({1, 0}).cp == U'i');
}

TEST_CASE("CharGrid: to_string header format") {
    const CharGrid g(10, 4);
    auto s = g.to_string();
    CHECK(s.starts_with("COLS=10 ROWS=4\n"));
}

TEST_CASE("CharGrid: from_string returns nullopt on garbage") {
    CHECK_FALSE(CharGrid::from_string("garbage").has_value());
    CHECK_FALSE(CharGrid::from_string("COLS=0 ROWS=0\n").has_value());
    CHECK_FALSE(CharGrid::from_string("").has_value());
}

TEST_CASE("CharGrid: all style flags serialized correctly") {
    CharGrid g(1, 1);
    ColorAttr a;
    a.bold = true;
    a.italic = true;
    a.underline = true;
    a.strike = true;
    g.put({0, 0}, U'Z', a);
    auto s = g.to_string();
    auto g2 = CharGrid::from_string(s);
    REQUIRE(g2.has_value());
    const auto& attr = g2->at({0, 0}).attr;
    CHECK(attr.bold);
    CHECK(attr.italic);
    CHECK(attr.underline);
    CHECK(attr.strike);
}

TEST_CASE("CharGrid::blit copies every src cell to dst at the given origin") {
    CharGrid src(2, 2);
    src.put({0, 0}, U'A', {});
    src.put({1, 0}, U'B', {});
    src.put({0, 1}, U'C', {});
    src.put({1, 1}, U'D', {});

    CharGrid dst(5, 5);
    dst.blit(src, {2, 1});

    CHECK(dst.at({2, 1}).cp == U'A');
    CHECK(dst.at({3, 1}).cp == U'B');
    CHECK(dst.at({2, 2}).cp == U'C');
    CHECK(dst.at({3, 2}).cp == U'D');
}

TEST_CASE("CharGrid::blit leaves cells outside src's footprint untouched") {
    CharGrid src(1, 1);
    src.put({0, 0}, U'X', {});
    CharGrid dst(3, 3);
    const auto before = dst.at({0, 0});
    dst.blit(src, {1, 1});
    CHECK(dst.at({0, 0}).cp == before.cp);
    CHECK(dst.at({1, 1}).cp == U'X');
}

TEST_CASE("CharGrid::blit clips a src that extends past dst's bounds without throwing") {
    CharGrid src(3, 3);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) { src.put({c, r}, U'#', {}); }
    }
    CharGrid dst(4, 4);
    dst.blit(src, {2, 2});  // extends to (4,4), one past dst's (0..3, 0..3)
    CHECK(dst.at({2, 2}).cp == U'#');
    CHECK(dst.at({3, 3}).cp == U'#');
}

TEST_CASE("CharGrid::blit clips a negative origin without throwing") {
    CharGrid src(3, 3);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) { src.put({c, r}, U'#', {}); }
    }
    CharGrid dst(4, 4);
    dst.blit(src, {-1, -1});
    CHECK(dst.at({0, 0}).cp == U'#');  // src's (1,1) lands at dst's (0,0)
}
