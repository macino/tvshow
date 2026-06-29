#include "tvshow/paint/paint.hpp"

#define Uses_TDrawBuffer
#include "tvshow/render/chargrid.hpp"

#include <tvision/tv.h>  // NOLINT(misc-include-cleaner) -- macro-gated umbrella header, see tvshow/paint/paint.hpp

#include <cstdint>
#include <string>

namespace tvshow::paint {

namespace {

// NOLINTNEXTLINE(misc-include-cleaner)
TColorDesired to_desired(uint32_t rgb) noexcept {
    return {TColorRGB(static_cast<uint8_t>(rgb >> 16U), static_cast<uint8_t>(rgb >> 8U),
                      static_cast<uint8_t>(rgb))};
}

// NOLINTNEXTLINE(misc-include-cleaner)
ushort style_flags(const render::ColorAttr& a) noexcept {
    ushort flags = 0;
    if (a.bold) {
        flags |= slBold;  // NOLINT(misc-include-cleaner)
    }
    if (a.italic) {
        flags |= slItalic;  // NOLINT(misc-include-cleaner)
    }
    if (a.underline) {
        flags |= slUnderline;  // NOLINT(misc-include-cleaner)
    }
    if (a.strike) {
        flags |= slStrike;  // NOLINT(misc-include-cleaner)
    }
    return flags;
}

// Encode one code point as UTF-8 into `out` (must have room for 5 bytes).
// Returns number of bytes written.
int utf8_encode_into(char32_t cp, char* out) {
    if (cp < 0x80) {
        out[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        out[0] = static_cast<char>(0xC0U | (cp >> 6U));
        out[1] = static_cast<char>(0x80U | (cp & 0x3FU));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = static_cast<char>(0xE0U | (cp >> 12U));
        out[1] = static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU));
        out[2] = static_cast<char>(0x80U | (cp & 0x3FU));
        return 3;
    }
    out[0] = static_cast<char>(0xF0U | (cp >> 18U));
    out[1] = static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU));
    out[2] = static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU));
    out[3] = static_cast<char>(0x80U | (cp & 0x3FU));
    return 4;
}

}  // namespace

void draw_row(const render::CharGrid& grid, int row, TDrawBuffer& buf) {
    for (int col = 0; col < grid.cols(); ++col) {
        const render::Cell& cell = grid.at({col, row});
        // NOLINTNEXTLINE(misc-include-cleaner)
        const TColorAttr attr(to_desired(cell.attr.fg), to_desired(cell.attr.bg),
                              style_flags(cell.attr));
        char utf8[5] = {};
        const int utf8_len = utf8_encode_into(cell.cp, utf8);
        buf.moveStr(static_cast<ushort>(col), TStringView(utf8, static_cast<size_t>(utf8_len)), attr, 1);
    }
}

}  // namespace tvshow::paint
