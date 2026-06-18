#include "tvshow/render/render.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/images/renderer.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/form.hpp"
#include "tvshow/layout/inline_text.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"
#include "tvshow/util/url.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
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
    const int r_end = std::min(rect.origin.row + rect.size.rows, grid.rows());
    const int c_end = std::min(rect.origin.col + rect.size.cols, grid.cols());
    for (int r = std::max(rect.origin.row, 0); r < r_end; ++r) {
        for (int c = std::max(rect.origin.col, 0); c < c_end; ++c) {
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
        if (pos.col < 0 || pos.col >= grid.cols() || pos.row < 0 || pos.row >= grid.rows()) {
            return;
        }
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

constexpr uint32_t k_visited_fg = 0xCC55CCU;  // purple for visited links

// SPEC §10.2: line boxes are built (and placed) by layout::place_inline, so
// layout's row count and render's character placement agree by construction.
void paint_text(CharGrid& grid, const layout::CellRect& content_box, const style::StyledNode& sn,
                const RenderOpts& opts) {
    for (const auto& placed : layout::place_inline(sn, content_box)) {
        if (placed.pos.col < 0 || placed.pos.col >= grid.cols() || placed.pos.row < 0 ||
            placed.pos.row >= grid.rows()) {
            continue;
        }
        const uint32_t bg = grid.at(placed.pos).attr.bg;
        ColorAttr attr = text_attr(*placed.token.style, bg);
        if (opts.visited_urls != nullptr && !placed.token.href.empty()) {
            const std::string resolved = util::resolve_url(opts.base_url, placed.token.href);
            if (opts.visited_urls->count(resolved) > 0) {
                attr.fg = k_visited_fg;
            }
        }
        grid.put(placed.pos, placed.token.cp, attr);
    }
}

// ── form controls ─────────────────────────────────────────────────────────────

// Bounds-checked cell writer with an absolute origin baked in.
struct GridPen {
    CharGrid* grid;
    int x0;
    int y0;

    void put(Point rel, char32_t cp) const {
        const Point abs{x0 + rel.col, y0 + rel.row};
        if (abs.col >= 0 && abs.col < grid->cols() && abs.row >= 0 && abs.row < grid->rows()) {
            grid->put(abs, cp, grid->at(abs).attr);
        }
    }
};

// Boolean HTML attribute presence: attr() returns null data_ptr when absent.
bool has_bool_attr(const dom::Node& node, std::string_view name) noexcept {
    return node.attr(name).data() != nullptr;
}

std::string_view resolve_text_value(const dom::Node& node, const FormValues& fv) noexcept {
    const auto it = fv.text.find(&node);
    return (it != fv.text.end()) ? std::string_view(it->second) : node.attr("value");
}

bool resolve_checked(const dom::Node& node, const FormValues& fv) noexcept {
    const auto it = fv.checked.find(&node);
    return (it != fv.checked.end()) ? it->second : has_bool_attr(node, "checked");
}

void paint_text_field(const GridPen& pen, int w, const dom::Node& node, const FormValues& fv,
                      bool password) {
    if (w < 2) {
        return;
    }
    pen.put({0, 0}, U'[');
    pen.put({w - 1, 0}, U']');
    const std::string_view val = resolve_text_value(node, fv);
    int col = 1;
    for (size_t i = 0; i < val.size() && col < w - 1; ++col, ++i) {
        pen.put({col, 0},
                password ? U'*' : static_cast<char32_t>(static_cast<unsigned char>(val[i])));
    }
    for (; col < w - 1; ++col) {
        pen.put({col, 0}, U' ');
    }
}

void paint_checkbox(const GridPen& pen, const dom::Node& node, const FormValues& fv) {
    pen.put({0, 0}, U'[');
    pen.put({1, 0}, resolve_checked(node, fv) ? U'x' : U' ');
    pen.put({2, 0}, U']');
}

void paint_radio(const GridPen& pen, const dom::Node& node, const FormValues& fv) {
    pen.put({0, 0}, U'(');
    pen.put({1, 0}, resolve_checked(node, fv) ? U'•' : U' ');  // U+2022 BULLET •
    pen.put({2, 0}, U')');
}

void paint_submit(const GridPen& pen, int w, const dom::Node& node) {
    if (w < 2) {
        return;
    }
    const std::string_view dom_label = node.attr("value");
    const std::string_view label = dom_label.empty() ? std::string_view("Submit") : dom_label;
    pen.put({0, 0}, U'[');
    pen.put({1, 0}, U' ');
    int col = 2;
    for (size_t i = 0; i < label.size() && col < w - 1; ++col, ++i) {
        pen.put({col, 0}, static_cast<char32_t>(static_cast<unsigned char>(label[i])));
    }
    pen.put({w - 1, 0}, U']');
}

void paint_textarea(const GridPen& pen, int w, int h, const dom::Node& node, const FormValues& fv) {
    if (w < 2 || h < 2) {
        return;
    }
    pen.put({0, 0}, U'┌');
    for (int c = 1; c < w - 1; ++c) {
        pen.put({c, 0}, U'─');
    }
    pen.put({w - 1, 0}, U'┐');
    for (int r = 1; r < h - 1; ++r) {
        pen.put({0, r}, U'│');
        for (int c = 1; c < w - 1; ++c) {
            pen.put({c, r}, U' ');
        }
        pen.put({w - 1, r}, U'│');
    }
    pen.put({0, h - 1}, U'└');
    for (int c = 1; c < w - 1; ++c) {
        pen.put({c, h - 1}, U'─');
    }
    pen.put({w - 1, h - 1}, U'┘');
    // Render text content in the interior (rows 1..h-2, cols 1..w-2).
    const std::string_view val = resolve_text_value(node, fv);
    int text_row = 1;
    size_t pos = 0;
    while (text_row < h - 1 && pos <= val.size()) {
        const size_t eol = val.find('\n', pos);
        const size_t line_end = (eol == std::string_view::npos) ? val.size() : eol;
        int col = 1;
        for (size_t i = pos; i < line_end && col < w - 1; ++i, ++col) {
            pen.put({col, text_row}, static_cast<char32_t>(static_cast<unsigned char>(val[i])));
        }
        pos = (eol == std::string_view::npos) ? val.size() + 1 : eol + 1;
        ++text_row;
    }
}

// Returns the display label for the currently selected <option> within node.
// Falls back to the first option, then empty if no options exist.
std::string_view get_select_label(const dom::Node& node, const FormValues& fv) noexcept {
    const std::string_view cur_val = resolve_text_value(node, fv);
    const dom::Node* first_opt = nullptr;
    const dom::Node* selected_opt = nullptr;
    for (const auto& cp : node.children) {
        if (cp->kind != dom::NodeKind::Element || cp->tag != "option") {
            continue;
        }
        if (first_opt == nullptr) {
            first_opt = cp.get();
        }
        if (!cur_val.empty()) {
            if (cp->attr("value") == cur_val) {
                selected_opt = cp.get();
                break;
            }
        } else if (selected_opt == nullptr && has_bool_attr(*cp, "selected")) {
            selected_opt = cp.get();
        }
    }
    const dom::Node* opt = (selected_opt != nullptr) ? selected_opt : first_opt;
    if (opt != nullptr) {
        for (const auto& tc : opt->children) {
            if (tc->kind == dom::NodeKind::Text) {
                return std::string_view(tc->text);
            }
        }
    }
    return {};
}

void paint_select(const GridPen& pen, int w, const dom::Node& node, const FormValues& fv) {
    if (w < 3) {
        return;
    }
    const std::string_view label = get_select_label(node, fv);
    pen.put({0, 0}, U'[');
    int col = 1;
    for (size_t i = 0; i < label.size() && col < w - 2; ++col, ++i) {
        pen.put({col, 0}, static_cast<char32_t>(static_cast<unsigned char>(label[i])));
    }
    for (; col < w - 2; ++col) {
        pen.put({col, 0}, U' ');
    }
    pen.put({w - 2, 0}, U'▾');  // U+25BE BLACK DOWN-POINTING SMALL TRIANGLE ▾
    pen.put({w - 1, 0}, U']');
}

void paint_form_control(CharGrid& grid, const layout::Box& box, const dom::Node& node,
                        const FormValues& fv) {
    const layout::FormControlKind kind = layout::form_control_kind(node);
    if (kind == layout::FormControlKind::None || kind == layout::FormControlKind::Hidden) {
        return;
    }
    const GridPen pen{&grid, box.border_box.origin.col, box.border_box.origin.row};
    const int w = box.border_box.size.cols;
    const int h = box.border_box.size.rows;
    switch (kind) {
    case layout::FormControlKind::Text:
        paint_text_field(pen, w, node, fv, false);
        break;
    case layout::FormControlKind::Password:
        paint_text_field(pen, w, node, fv, true);
        break;
    case layout::FormControlKind::Checkbox:
        paint_checkbox(pen, node, fv);
        break;
    case layout::FormControlKind::Radio:
        paint_radio(pen, node, fv);
        break;
    case layout::FormControlKind::Submit:
        paint_submit(pen, w, node);
        break;
    case layout::FormControlKind::Textarea:
        paint_textarea(pen, w, h, node, fv);
        break;
    case layout::FormControlKind::Select:
        paint_select(pen, w, node, fv);
        break;
    case layout::FormControlKind::None:
    case layout::FormControlKind::Hidden:
        break;
    }
}

// ── image rendering ──────────────────────────────────────────────────────────

void paint_img(CharGrid& grid, const layout::Box& box, const dom::Node& node,
               const images::ImageRenderer& renderer) {
    const int cols = box.border_box.size.cols;
    const int rows = box.border_box.size.rows;
    const std::string_view alt = node.attr("alt");
    const std::string_view src = node.attr("src");
    const std::vector<std::string> lines = renderer.render(cols, rows, alt, src);
    const int x0 = box.border_box.origin.col;
    const int y0 = box.border_box.origin.row;
    for (int r = 0; r < static_cast<int>(lines.size()) && r < rows; ++r) {
        const auto& line = lines[static_cast<std::size_t>(r)];
        for (int c = 0; c < static_cast<int>(line.size()) && c < cols; ++c) {
            const Point pos{x0 + c, y0 + r};
            if (pos.col >= 0 && pos.col < grid.cols() && pos.row >= 0 && pos.row < grid.rows()) {
                const ColorAttr attr = grid.at(pos).attr;
                grid.put(pos,
                         static_cast<char32_t>(
                             static_cast<unsigned char>(line[static_cast<std::size_t>(c)])),
                         attr);
            }
        }
    }
}

// ── box tree walk ────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
void paint_box(CharGrid& grid, const layout::Box& box, const FormValues& fv,
               const images::ImageRenderer& img_renderer, const RenderOpts& opts) {
    if (box.node == nullptr) {
        return;
    }
    const auto& st = box.node->style;
    if (st.visibility != style::Visibility::Hidden) {
        paint_background(grid, box.border_box, st.background);
        if (box.node->node != nullptr) {
            const layout::FormControlKind kind = layout::form_control_kind(*box.node->node);
            if (kind != layout::FormControlKind::None && kind != layout::FormControlKind::Hidden) {
                paint_form_control(grid, box, *box.node->node, fv);
                return;
            }
            if (box.node->node->tag == "img") {
                paint_img(grid, box, *box.node->node, img_renderer);
                return;
            }
        }
        paint_border(grid, box.border_box, st.border);
        paint_text(grid, box.content_box, *box.node, opts);
    }
    for (const auto& child : box.children) {
        paint_box(grid, child, fv, img_renderer, opts);
    }
}

}  // namespace

CharGrid render(const layout::Box& root, const FormValues& fv, const RenderOpts& opts) {
    CharGrid grid(std::max(root.border_box.size.cols, 1), std::max(root.border_box.size.rows, 1));
    const images::AltTextRenderer img_renderer;
    paint_box(grid, root, fv, img_renderer, opts);
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
