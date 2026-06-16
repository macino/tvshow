#include "tvshow/paint/paint.hpp"

#define Uses_TDrawBuffer
#include "tvshow/render/chargrid.hpp"

#include <tvision/tv.h>

#include <doctest/doctest.h>

using tvshow::render::CharGrid;
using tvshow::render::ColorAttr;

namespace {

// TDrawBuffer keeps its cell array protected (only TView/TSystemError are
// friends); expose it here so the test can read back what draw_row wrote.
struct TestDrawBuffer : public TDrawBuffer {
    using TDrawBuffer::data;
};

}  // namespace

TEST_CASE("paint: draw_row writes the grid's characters") {
    CharGrid grid(3, 1);
    grid.put({0, 0}, U'a', ColorAttr{});
    grid.put({1, 0}, U'b', ColorAttr{});
    grid.put({2, 0}, U'c', ColorAttr{});

    TestDrawBuffer buf;
    tvshow::paint::draw_row(grid, 0, buf);

    CHECK(buf.data[0]._ch.getText() == "a");
    CHECK(buf.data[1]._ch.getText() == "b");
    CHECK(buf.data[2]._ch.getText() == "c");
}

TEST_CASE("paint: draw_row carries the foreground/background RGB") {
    CharGrid grid(1, 1);
    ColorAttr attr;
    attr.fg = 0x00FF00U;
    attr.bg = 0xFF0000U;
    grid.put({0, 0}, U'x', attr);

    TestDrawBuffer buf;
    tvshow::paint::draw_row(grid, 0, buf);

    // NOLINTBEGIN(misc-include-cleaner)
    const TColorAttr& got = getAttr(buf.data[0]);
    CHECK(TColorRGB(getFore(got).asRGB()) == TColorRGB(0x00, 0xFF, 0x00));
    CHECK(TColorRGB(getBack(got).asRGB()) == TColorRGB(0xFF, 0x00, 0x00));
    // NOLINTEND(misc-include-cleaner)
}

TEST_CASE("paint: draw_row maps style flags to tvision style bits") {
    CharGrid grid(1, 1);
    ColorAttr attr;
    attr.bold = true;
    attr.underline = true;
    grid.put({0, 0}, U'x', attr);

    TestDrawBuffer buf;
    tvshow::paint::draw_row(grid, 0, buf);

    // NOLINTBEGIN(misc-include-cleaner)
    const TColorAttr& got = getAttr(buf.data[0]);
    CHECK((getStyle(got) & slBold) != 0);
    CHECK((getStyle(got) & slUnderline) != 0);
    CHECK((getStyle(got) & slItalic) == 0);
    // NOLINTEND(misc-include-cleaner)
}

TEST_CASE("paint: draw_row encodes non-ASCII code points as UTF-8") {
    CharGrid grid(1, 1);
    grid.put({0, 0}, U'┌', ColorAttr{});

    TestDrawBuffer buf;
    tvshow::paint::draw_row(grid, 0, buf);

    CHECK(buf.data[0]._ch.getText() == "┌");
}
