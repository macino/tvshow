#include "tvshow/render/render.hpp"

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/inline_text.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

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

// SPEC §10.2: line boxes are built (and placed) by layout::place_inline, so
// layout's row count and render's character placement agree by construction.
void paint_text(CharGrid& grid, const layout::CellRect& content_box, const style::StyledNode& sn) {
    for (const auto& placed : layout::place_inline(sn, content_box)) {
        const uint32_t bg = grid.at(placed.pos).attr.bg;
        grid.put(placed.pos, placed.token.cp, text_attr(*placed.token.style, bg));
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
        paint_text(grid, box.content_box, *box.node);
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

void apply_focus(CharGrid& grid, const std::vector<layout::CellRect>& spans) {
    for (const auto& span : spans) {
        const int row_end = std::min(span.origin.row + span.size.rows, grid.rows());
        const int col_end = std::min(span.origin.col + span.size.cols, grid.cols());
        for (int r = std::max(span.origin.row, 0); r < row_end; ++r) {
            for (int c = std::max(span.origin.col, 0); c < col_end; ++c) {
                const Point pos{c, r};
                const Cell cell = grid.at(pos);
                ColorAttr attr = cell.attr;
                std::swap(attr.fg, attr.bg);
                grid.put(pos, cell.cp, attr);
            }
        }
    }
}

}  // namespace tvshow::render
