#include "tvshow/render/render.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tvshow::render {

namespace {

constexpr uint32_t k_default_fg = 0xFFFFFFU;

uint32_t to_rgb_u32(const style::Color& c) noexcept {
    return (static_cast<uint32_t>(c.r) << 16U) | (static_cast<uint32_t>(c.g) << 8U) |
           static_cast<uint32_t>(c.b);
}

ColorAttr text_attr(const style::ComputedStyle& st, uint32_t bg) noexcept {
    ColorAttr attr;
    attr.bg = bg;
    attr.fg = st.color.none ? k_default_fg : to_rgb_u32(st.color);
    attr.bold = st.font_weight == style::FontWeight::Bold;
    attr.italic = st.font_style == style::FontStyle::Italic;
    attr.underline = st.text_decoration == style::TextDecoration::Underline;
    attr.strike = st.text_decoration == style::TextDecoration::LineThrough;
    return attr;
}

// Decode one UTF-8 code point starting at s[i]; advance i past the bytes consumed.
char32_t utf8_decode_at(std::string_view s, size_t& i) noexcept {
    const auto lead = static_cast<uint8_t>(s[i]);
    if (lead < 0x80U) {
        ++i;
        return static_cast<char32_t>(lead);
    }
    auto cont = [&](size_t pos) -> uint8_t {
        return pos < s.size() ? static_cast<uint8_t>(s[pos]) : 0x80U;
    };
    if ((lead & 0xE0U) == 0xC0U && i + 1 < s.size()) {
        const char32_t cp = ((lead & 0x1FU) << 6U) | (cont(i + 1) & 0x3FU);
        i += 2;
        return cp;
    }
    if ((lead & 0xF0U) == 0xE0U && i + 2 < s.size()) {
        const char32_t cp =
            ((lead & 0x0FU) << 12U) | ((cont(i + 1) & 0x3FU) << 6U) | (cont(i + 2) & 0x3FU);
        i += 3;
        return cp;
    }
    if ((lead & 0xF8U) == 0xF0U && i + 3 < s.size()) {
        const char32_t cp = ((lead & 0x07U) << 18U) | ((cont(i + 1) & 0x3FU) << 12U) |
                            ((cont(i + 2) & 0x3FU) << 6U) | (cont(i + 3) & 0x3FU);
        i += 4;
        return cp;
    }
    ++i;
    return U'?';
}

// ── background ───────────────────────────────────────────────────────────────

void paint_background(CharGrid& grid, const layout::CellRect& rect, const style::Color& bg) {
    if (bg.none) {
        return;
    }
    const uint32_t bg_u32 = to_rgb_u32(bg);
    for (int r = rect.origin.row; r < rect.origin.row + rect.size.rows; ++r) {
        for (int c = rect.origin.col; c < rect.origin.col + rect.size.cols; ++c) {
            const Point pos{c, r};
            ColorAttr attr = grid.at(pos).attr;
            attr.bg = bg_u32;
            grid.put(pos, grid.at(pos).cp, attr);
        }
    }
}

// ── border ───────────────────────────────────────────────────────────────────

struct BorderGlyphs {
    char32_t horizontal;
    char32_t vertical;
    char32_t top_left;
    char32_t top_right;
    char32_t bottom_left;
    char32_t bottom_right;
};

// SPEC §9: dotted/ridge/groove/inset/outset fall back to solid (terminals
// lack the glyphs); dashed uses solid corners with dashed edges.
BorderGlyphs glyphs_for(style::BorderStyle s) noexcept {
    if (s == style::BorderStyle::Double) {
        return {U'═', U'║', U'╔', U'╗', U'╚', U'╝'};
    }
    if (s == style::BorderStyle::Dashed) {
        return {U'╌', U'╎', U'┌', U'┐', U'└', U'┘'};
    }
    return {U'─', U'│', U'┌', U'┐', U'└', U'┘'};
}

void paint_border(CharGrid& grid, const layout::CellRect& rect, const style::BorderBox& border) {
    const auto& top = border[0];
    const auto& right = border[1];
    const auto& bottom = border[2];
    const auto& left = border[3];

    const int x0 = rect.origin.col;
    const int y0 = rect.origin.row;
    const int x1 = rect.origin.col + rect.size.cols - 1;
    const int y1 = rect.origin.row + rect.size.rows - 1;

    auto put_glyph = [&](Point pos, char32_t cp, const style::Color& color) {
        ColorAttr attr = grid.at(pos).attr;
        attr.fg = color.none ? k_default_fg : to_rgb_u32(color);
        grid.put(pos, cp, attr);
    };

    if (top.style != style::BorderStyle::None) {
        const auto g = glyphs_for(top.style);
        for (int c = x0; c <= x1; ++c) {
            put_glyph({c, y0}, g.horizontal, top.color);
        }
    }
    if (bottom.style != style::BorderStyle::None) {
        const auto g = glyphs_for(bottom.style);
        for (int c = x0; c <= x1; ++c) {
            put_glyph({c, y1}, g.horizontal, bottom.color);
        }
    }
    if (left.style != style::BorderStyle::None) {
        const auto g = glyphs_for(left.style);
        for (int r = y0; r <= y1; ++r) {
            put_glyph({x0, r}, g.vertical, left.color);
        }
    }
    if (right.style != style::BorderStyle::None) {
        const auto g = glyphs_for(right.style);
        for (int r = y0; r <= y1; ++r) {
            put_glyph({x1, r}, g.vertical, right.color);
        }
    }
    if (top.style != style::BorderStyle::None && left.style != style::BorderStyle::None) {
        put_glyph({x0, y0}, glyphs_for(top.style).top_left, top.color);
    }
    if (top.style != style::BorderStyle::None && right.style != style::BorderStyle::None) {
        put_glyph({x1, y0}, glyphs_for(top.style).top_right, top.color);
    }
    if (bottom.style != style::BorderStyle::None && left.style != style::BorderStyle::None) {
        put_glyph({x0, y1}, glyphs_for(bottom.style).bottom_left, bottom.color);
    }
    if (bottom.style != style::BorderStyle::None && right.style != style::BorderStyle::None) {
        put_glyph({x1, y1}, glyphs_for(bottom.style).bottom_right, bottom.color);
    }
}

// ── inline text ──────────────────────────────────────────────────────────────

struct TextCursor {
    layout::CellRect content_box;
    int col = 0;
    int row = 0;
};

void paint_codepoint(CharGrid& grid, TextCursor& cur, char32_t cp, const style::ComputedStyle& st) {
    const int max_col = cur.content_box.origin.col + cur.content_box.size.cols;
    const int max_row = cur.content_box.origin.row + cur.content_box.size.rows;
    if (cur.row >= max_row) {
        return;  // clipped — content overflows its box
    }
    if (cp == U'\n') {
        ++cur.row;
        cur.col = cur.content_box.origin.col;
        return;
    }
    if (cur.col >= max_col) {
        ++cur.row;
        cur.col = cur.content_box.origin.col;
        if (cur.row >= max_row) {
            return;
        }
    }
    const Point pos{cur.col, cur.row};
    const uint32_t bg = grid.at(pos).attr.bg;
    grid.put(pos, cp, text_attr(st, bg));
    ++cur.col;
}

// Mirrors layout::count_inline_chars's recursion condition (Text nodes, plus
// any non-block/flex/none descendant) so wrapped row counts stay consistent
// between the layout and render stages.
// NOLINTNEXTLINE(misc-no-recursion)
void paint_inline(CharGrid& grid, TextCursor& cur, const style::StyledNode& sn) {
    for (const auto& child : sn.children) {
        if (child.node == nullptr) {
            continue;
        }
        if (child.node->kind == dom::NodeKind::Text) {
            size_t i = 0;
            const std::string_view text = child.node->text;
            while (i < text.size()) {
                paint_codepoint(grid, cur, utf8_decode_at(text, i), child.style);
            }
        } else if (child.style.display != style::Display::None &&
                   child.style.display != style::Display::Block &&
                   child.style.display != style::Display::Flex) {
            paint_inline(grid, cur, child);
        }
    }
}

// ── box tree walk ────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
void paint_box(CharGrid& grid, const layout::Box& box) {
    if (box.node == nullptr) {
        return;
    }
    const auto& st = box.node->style;
    if (st.visibility != style::Visibility::Hidden) {
        paint_background(grid, box.border_box, st.background);
        paint_border(grid, box.border_box, st.border);
        TextCursor cur{box.content_box, box.content_box.origin.col, box.content_box.origin.row};
        paint_inline(grid, cur, *box.node);
    }
    for (const auto& child : box.children) {
        paint_box(grid, child);
    }
}

}  // namespace

CharGrid render(const layout::Box& root) {
    CharGrid grid(std::max(root.border_box.size.cols, 1), std::max(root.border_box.size.rows, 1));
    paint_box(grid, root);
    return grid;
}

}  // namespace tvshow::render
