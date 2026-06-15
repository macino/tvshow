#include "tvshow/layout/types.hpp"
#include "tvshow/types.hpp"

#include <doctest/doctest.h>

#include <type_traits>

using tvshow::Point;
using tvshow::Rect;
using tvshow::Size;
using tvshow::layout::CellPos;
using tvshow::layout::CellRect;
using tvshow::layout::Viewport;

TEST_CASE("Point: default values") {
    Point p;
    CHECK(p.col == 0);
    CHECK(p.row == 0);
}

TEST_CASE("Point: equality") {
    CHECK(Point{1, 2} == Point{1, 2});
    CHECK_FALSE(Point{1, 2} == Point{2, 1});
}

TEST_CASE("Size: default values") {
    Size s;
    CHECK(s.cols == 0);
    CHECK(s.rows == 0);
}

TEST_CASE("Rect: empty when size is zero") {
    CHECK(Rect{}.empty());
    CHECK(Rect{{0, 0}, {0, 5}}.empty());
    CHECK(Rect{{0, 0}, {5, 0}}.empty());
    CHECK_FALSE(Rect{{0, 0}, {1, 1}}.empty());
}

TEST_CASE("Rect: contains") {
    const Rect r{{2, 3}, {4, 5}};
    CHECK(r.contains({2, 3}));        // top-left corner
    CHECK(r.contains({5, 7}));        // bottom-right - 1
    CHECK_FALSE(r.contains({6, 7}));  // one past right
    CHECK_FALSE(r.contains({5, 8}));  // one past bottom
    CHECK_FALSE(r.contains({1, 3}));  // left of origin
    CHECK_FALSE(r.contains({2, 2}));  // above origin
}

TEST_CASE("Rect: equality") {
    CHECK(Rect{{1, 2}, {3, 4}} == Rect{{1, 2}, {3, 4}});
    CHECK_FALSE(Rect{{1, 2}, {3, 4}} == Rect{{1, 2}, {3, 5}});
}

TEST_CASE("layout aliases: CellPos is Point") {
    static_assert(std::is_same_v<CellPos, Point>);
    CellPos p{5, 10};
    CHECK(p.col == 5);
    CHECK(p.row == 10);
}

TEST_CASE("layout aliases: Viewport is Size") {
    static_assert(std::is_same_v<Viewport, Size>);
    Viewport vp{80, 24};
    CHECK(vp.cols == 80);
    CHECK(vp.rows == 24);
}

TEST_CASE("layout aliases: CellRect is Rect") {
    static_assert(std::is_same_v<CellRect, Rect>);
    const CellRect cr{{0, 0}, {80, 24}};
    CHECK_FALSE(cr.empty());
    CHECK(cr.contains({0, 0}));
    CHECK(cr.contains({79, 23}));
    CHECK_FALSE(cr.contains({80, 23}));
}
