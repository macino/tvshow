#include "tvshow/layout/engine.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/form.hpp"
#include "tvshow/layout/inline_text.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace tvshow::layout {

namespace {

// ── Unit resolution ───────────────────────────────────────────────────────────

// SPEC §7.3: 1px = 1/8 col, 1ch = 1 col, 1em = 1 col (horizontal).
// auto → 0 (callers that need auto-fill handle it themselves).
int resolve_h(const style::Length& l, int avail) noexcept {
    if (l.is_auto) {
        return 0;
    }
    switch (l.unit) {
    case style::LengthUnit::Px:
        return static_cast<int>(std::floor(l.value / 8.0F + 0.5F));
    case style::LengthUnit::Pct:
        return static_cast<int>(std::floor(l.value / 100.0F * static_cast<float>(avail) + 0.5F));
    case style::LengthUnit::Ch:
    case style::LengthUnit::Em:
    case style::LengthUnit::Rem:
        return static_cast<int>(std::floor(l.value + 0.5F));
    }
    return 0;
}

// SPEC §7.3: 1px = 1/16 row, 1em = 1 row (vertical).
// auto → 0.
int resolve_v(const style::Length& l, int avail) noexcept {
    if (l.is_auto) {
        return 0;
    }
    switch (l.unit) {
    case style::LengthUnit::Px:
        return static_cast<int>(std::floor(l.value / 16.0F + 0.5F));
    case style::LengthUnit::Pct:
        return static_cast<int>(std::floor(l.value / 100.0F * static_cast<float>(avail) + 0.5F));
    case style::LengthUnit::Ch:
    case style::LengthUnit::Em:
    case style::LengthUnit::Rem:
        return static_cast<int>(std::floor(l.value + 0.5F));
    }
    return 0;
}

// ── Border helpers ────────────────────────────────────────────────────────────

// SPEC §9: border width is always 1 cell when border-style != none.
int border_cell(const style::BorderSide& side) noexcept {
    return (side.style != style::BorderStyle::None) ? 1 : 0;
}

// ── Box dimension structs ─────────────────────────────────────────────────────

struct EdgePx {
    int top = 0, right = 0, bottom = 0, left = 0;
};

EdgePx compute_border(const style::BorderBox& border) noexcept {
    return {border_cell(border[0]), border_cell(border[1]), border_cell(border[2]),
            border_cell(border[3])};
}

EdgePx compute_padding(const style::Edges& pad, int avail_w, int avail_h) noexcept {
    return {resolve_v(pad.top, avail_h), resolve_h(pad.right, avail_w),
            resolve_v(pad.bottom, avail_h), resolve_h(pad.left, avail_w)};
}

EdgePx compute_margin(const style::Edges& mar, int avail_w, int avail_h) noexcept {
    return {resolve_v(mar.top, avail_h), resolve_h(mar.right, avail_w),
            resolve_v(mar.bottom, avail_h), resolve_h(mar.left, avail_w)};
}

// ── Inline content measurement ────────────────────────────────────────────────

bool has_inline_content(const style::StyledNode& sn) noexcept {
    return std::ranges::any_of(sn.children, [](const style::StyledNode& child) {
        if (child.node == nullptr) {
            return false;
        }
        if (child.node->kind == dom::NodeKind::Text) {
            return true;
        }
        return child.style.display == style::Display::Inline ||
               child.style.display == style::Display::InlineBlock;
    });
}

// ── Image sizing ──────────────────────────────────────────────────────────────

// Parse an HTML attribute value as a plain integer (bare number = pixels).
// Returns -1 if not parseable.
int parse_attr_int(std::string_view s) noexcept {
    if (s.empty()) {
        return -1;
    }
    int v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size()) {
        return -1;
    }
    return v;
}

// SPEC §6.5: width/height attrs are in px. Map via SPEC §7.3 ratios.
// cols = round(px_w / 8), rows = round(px_h / 16).
// If width attr absent: cols = len("[alt]") = alt.size() + 2, min 1.
// If height attr absent: rows = 1.
struct ImgSize {
    int cols;
    int rows;
};

ImgSize img_cell_size(const dom::Node& node) noexcept {
    const std::string_view w_attr = node.attr("width");
    const std::string_view h_attr = node.attr("height");
    const std::string_view alt = node.attr("alt");

    int cols = 0;
    const int w_px = parse_attr_int(w_attr);
    if (w_px > 0) {
        cols = static_cast<int>(std::floor(static_cast<float>(w_px) / 8.0F + 0.5F));
    } else {
        cols = static_cast<int>(alt.size()) + 2;  // "[alt]"
    }
    cols = std::max(1, cols);

    int rows = 1;
    const int h_px = parse_attr_int(h_attr);
    if (h_px > 0) {
        rows = static_cast<int>(std::floor(static_cast<float>(h_px) / 16.0F + 0.5F));
        rows = std::max(1, rows);
    }
    return {cols, rows};
}

// ── Forward declarations (mutual recursion) ───────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
Box layout_block(const style::StyledNode& sn, CellPos origin, int avail_w, int avail_h);
// NOLINTNEXTLINE(misc-no-recursion)
Box layout_flex(const style::StyledNode& sn, CellPos origin, int avail_w, int avail_h);

// NOLINTNEXTLINE(misc-no-recursion)
Box layout_node(const style::StyledNode& sn, CellPos origin, int avail_w, int avail_h) {
    if (sn.style.display == style::Display::Flex) {
        return layout_flex(sn, origin, avail_w, avail_h);
    }
    return layout_block(sn, origin, avail_w, avail_h);
}

// ── Block layout ──────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
Box layout_block(const style::StyledNode& sn, CellPos origin, int avail_w, int avail_h) {
    const auto& st = sn.style;
    const EdgePx brd = compute_border(st.border);
    const EdgePx pad = compute_padding(st.padding, avail_w, avail_h);
    const EdgePx mar = compute_margin(st.margin, avail_w, avail_h);

    // Horizontal chrome: margin + border + padding on each side.
    const int chrome_h = mar.left + brd.left + pad.left + pad.right + brd.right + mar.right;

    // Content width: auto = stretch to fill, explicit = use value.
    const int content_w =
        st.width.is_auto ? std::max(0, avail_w - chrome_h) : resolve_h(st.width, avail_w);

    const CellPos border_origin{origin.col + mar.left, origin.row + mar.top};
    const CellPos content_origin{border_origin.col + brd.left + pad.left,
                                 border_origin.row + brd.top + pad.top};

    // Lay out block-level children.
    std::vector<Box> children;
    int y = content_origin.row;

    for (const auto& child : sn.children) {
        if (child.node == nullptr || child.node->kind != dom::NodeKind::Element) {
            continue;
        }
        // Form controls: leaf boxes with fixed sizes, no recursive layout.
        const FormControlKind fck = form_control_kind(*child.node);
        if (fck == FormControlKind::Hidden) {
            continue;
        }
        if (fck != FormControlKind::None) {
            if (child.style.display == style::Display::Block) {
                // Block-display controls (textarea): placed as a block child here.
                const auto [cols, rows] = form_control_size(*child.node);
                const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
                Box fc_box;
                fc_box.node = &child;
                fc_box.border_box = {{content_origin.col, y + cmar.top}, {cols, rows}};
                fc_box.content_box = fc_box.border_box;
                y += cmar.top + rows + cmar.bottom;
                children.push_back(std::move(fc_box));
            }
            // Inline/InlineBlock controls (input, button, select) participate in
            // the inline flow and are placed below via the break_inline scan.
            continue;
        }
        if (child.node->tag == "img") {
            const auto [cols, rows] = img_cell_size(*child.node);
            const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
            Box img_box;
            img_box.node = &child;
            img_box.border_box = {{content_origin.col, y + cmar.top}, {cols, rows}};
            img_box.content_box = img_box.border_box;
            y += cmar.top + rows + cmar.bottom;
            children.push_back(std::move(img_box));
            continue;
        }
        const auto disp = child.style.display;
        if (disp == style::Display::None) {
            continue;
        }
        if (disp == style::Display::Block || disp == style::Display::Flex ||
            disp == style::Display::InlineBlock) {
            const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
            Box child_box = layout_node(child, {content_origin.col, y}, content_w, avail_h);
            y += cmar.top + child_box.border_box.size.rows + cmar.bottom;
            children.push_back(std::move(child_box));
        }
    }

    if (has_inline_content(sn)) {
        const auto lines = break_inline(sn, content_w);
        // Create child boxes for any inline form controls found in the line flow.
        for (int line_idx = 0; line_idx < static_cast<int>(lines.size()); ++line_idx) {
            int col = 0;
            for (const auto& tok : lines[static_cast<size_t>(line_idx)]) {
                if (tok.widget != nullptr) {
                    const auto [wc, wr] = form_control_size(*tok.widget->node);
                    Box fc_box;
                    fc_box.node = tok.widget;
                    fc_box.border_box = {
                        {content_origin.col + col, content_origin.row + line_idx}, {wc, wr}};
                    fc_box.content_box = fc_box.border_box;
                    children.push_back(std::move(fc_box));
                }
                ++col;
            }
        }
        y += static_cast<int>(lines.size());
    }

    // Text browser: explicit height on block containers produces blank rows without
    // adding visible content.  img and form controls are already sized above.
    const int content_h = std::max(0, y - content_origin.row);

    Box box;
    box.node = &sn;
    box.border_box = {border_origin,
                      {brd.left + pad.left + content_w + pad.right + brd.right,
                       brd.top + pad.top + content_h + pad.bottom + brd.bottom}};
    box.content_box = {content_origin, {content_w, content_h}};
    box.children = std::move(children);
    return box;
}

// ── Flex layout ───────────────────────────────────────────────────────────────

struct FlexItem {
    const style::StyledNode* sn;
    int main_base;  // hypothetical main-axis size in cells
    float grow;
    float shrink;
};

// Collect flex items from a container's children, skipping display:none and
// hidden form controls. Returns items in source order.
std::vector<FlexItem> collect_flex_items(const style::StyledNode& sn, bool is_row, int content_main,
                                         int avail_cross) {
    std::vector<FlexItem> items;
    for (const auto& child : sn.children) {
        if (child.node == nullptr || child.node->kind != dom::NodeKind::Element) {
            continue;
        }
        if (child.style.display == style::Display::None) {
            continue;
        }
        const FormControlKind fck = form_control_kind(*child.node);
        if (fck == FormControlKind::Hidden) {
            continue;
        }
        if (child.node->tag == "img") {
            const auto [icols, irows] = img_cell_size(*child.node);
            const int base = is_row ? icols : irows;
            items.push_back({&child, base, child.style.flex_grow, child.style.flex_shrink});
            continue;
        }

        int base = 0;
        const auto& fb = child.style.flex_basis;
        if (!fb.is_auto) {
            base = is_row ? resolve_h(fb, content_main) : resolve_v(fb, avail_cross);
        } else if (is_row && !child.style.width.is_auto) {
            base = resolve_h(child.style.width, content_main);
        } else if (!is_row && !child.style.height.is_auto) {
            base = resolve_v(child.style.height, avail_cross);
        }
        items.push_back({&child, base, child.style.flex_grow, child.style.flex_shrink});
    }
    return items;
}

// Distribute main-axis free space among items via flex-grow / flex-shrink.
void distribute_flex_space(std::vector<int>& main_sizes, const std::vector<FlexItem>& items,
                           int free_space) {
    const int n = static_cast<int>(main_sizes.size());
    if (free_space > 0) {
        const float total_grow =
            std::accumulate(items.begin(), items.end(), 0.0F,
                            [](float s, const FlexItem& it) { return s + it.grow; });
        if (total_grow > 0) {
            for (int i = 0; i < n; ++i) {
                main_sizes[static_cast<size_t>(i)] +=
                    static_cast<int>(items[static_cast<size_t>(i)].grow / total_grow *
                                     static_cast<float>(free_space));
            }
        }
    } else if (free_space < 0) {
        const float total_ws =
            std::accumulate(items.begin(), items.end(), 0.0F, [](float s, const FlexItem& it) {
                return s + it.shrink * static_cast<float>(it.main_base);
            });
        if (total_ws > 0) {
            for (int i = 0; i < n; ++i) {
                const auto& it = items[static_cast<size_t>(i)];
                main_sizes[static_cast<size_t>(i)] +=
                    static_cast<int>(it.shrink * static_cast<float>(it.main_base) / total_ws *
                                     static_cast<float>(free_space));
                main_sizes[static_cast<size_t>(i)] =
                    std::max(0, main_sizes[static_cast<size_t>(i)]);
            }
        }
    }
}

// Compute per-item main-axis offsets from content origin based on justify-content.
// Returns {start_offset, gap_between_items} (gap_between adds to the fixed gap).
struct JustifyResult {
    int start = 0;
    int between = 0;
};
JustifyResult apply_justify(style::JustifyContent jc, int free_space, int n) {
    switch (jc) {
    case style::JustifyContent::FlexStart:
        return {0, 0};
    case style::JustifyContent::FlexEnd:
        return {free_space, 0};
    case style::JustifyContent::Center:
        return {free_space / 2, 0};
    case style::JustifyContent::SpaceBetween:
        return {0, (n > 1) ? free_space / (n - 1) : 0};
    case style::JustifyContent::SpaceAround:
        return {(n > 0) ? free_space / n / 2 : 0, (n > 0) ? free_space / n : 0};
    }
    return {0, 0};
}

// Apply cross-axis alignment (align-items) to already-laid-out flex items.
// content_cross: resolved container cross size.
// is_row: true for row direction, false for column direction.
void apply_cross_align(std::vector<Box>& children, style::AlignItems align, bool is_row,
                       int content_cross) {
    for (auto& cb : children) {
        const int item_cross = is_row ? cb.border_box.size.rows : cb.border_box.size.cols;
        const int free_cross = content_cross - item_cross;

        int cross_offset = 0;
        switch (align) {
        case style::AlignItems::Stretch:
            if (is_row && content_cross > item_cross) {
                cb.border_box.size.rows = content_cross;
                cb.content_box.size.rows = content_cross;
            }
            break;
        case style::AlignItems::FlexStart:
            break;
        case style::AlignItems::FlexEnd:
            cross_offset = free_cross;
            break;
        case style::AlignItems::Center:
            cross_offset = free_cross / 2;
            break;
        case style::AlignItems::Baseline:
            break;
        }

        if (cross_offset > 0) {
            if (is_row) {
                cb.border_box.origin.row += cross_offset;
                cb.content_box.origin.row += cross_offset;
            } else {
                cb.border_box.origin.col += cross_offset;
                cb.content_box.origin.col += cross_offset;
            }
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion,readability-function-cognitive-complexity)
Box layout_flex(const style::StyledNode& sn, CellPos origin, int avail_w, int avail_h) {
    const auto& st = sn.style;
    const EdgePx brd = compute_border(st.border);
    const EdgePx pad = compute_padding(st.padding, avail_w, avail_h);
    const EdgePx mar = compute_margin(st.margin, avail_w, avail_h);

    const int chrome_h = mar.left + brd.left + pad.left + pad.right + brd.right + mar.right;
    const int content_w =
        st.width.is_auto ? std::max(0, avail_w - chrome_h) : resolve_h(st.width, avail_w);

    const CellPos border_origin{origin.col + mar.left, origin.row + mar.top};
    const CellPos content_origin{border_origin.col + brd.left + pad.left,
                                 border_origin.row + brd.top + pad.top};

    const bool is_row = (st.flex_direction != style::FlexDirection::Column &&
                         st.flex_direction != style::FlexDirection::ColumnReverse);
    const int content_main = is_row ? content_w : avail_h;

    // For row: cross is height.  Text browser ignores explicit height (would create
    // blank rows); always -1 so content_cross = max child height.
    // For column: cross is width (always known = content_w).
    int content_cross_explicit = content_w;  // column direction: cross is content_w
    if (is_row) {
        content_cross_explicit = -1;  // always auto in text mode
    }

    const int gap_main = is_row ? resolve_h(st.gap, content_main) : resolve_v(st.gap, avail_h);

    const std::vector<FlexItem> items =
        collect_flex_items(sn, is_row, content_main, is_row ? avail_h : content_w);
    const int n = static_cast<int>(items.size());

    // ── Main-axis sizing ──────────────────────────────────────────────────────

    std::vector<int> main_sizes(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        main_sizes[static_cast<size_t>(i)] = items[static_cast<size_t>(i)].main_base;
    }

    const int total_base = std::accumulate(main_sizes.begin(), main_sizes.end(), 0);
    const int total_gaps = (n > 1) ? gap_main * (n - 1) : 0;
    // Row: distribute horizontal space. Column: use avail_h (vertical space available).
    const int main_available =
        is_row ? content_main : (st.height.is_auto ? avail_h : resolve_v(st.height, avail_h));
    distribute_flex_space(main_sizes, items, main_available - total_base - total_gaps);

    const int used_main = std::accumulate(main_sizes.begin(), main_sizes.end(), 0) + total_gaps;
    const int free_main = main_available - used_main;

    const auto [jc_start, jc_between] = apply_justify(st.justify_content, free_main, n);

    // ── Lay out each item ─────────────────────────────────────────────────────

    const bool do_stretch = (st.align_items == style::AlignItems::Stretch);
    const int cross_avail = is_row ? avail_h : content_w;

    std::vector<Box> children;
    children.reserve(static_cast<size_t>(n));

    int main_cursor = (is_row ? content_origin.col : content_origin.row) + jc_start;

    for (int i = 0; i < n; ++i) {
        const auto& item = items[static_cast<size_t>(i)];
        const int msz = main_sizes[static_cast<size_t>(i)];

        int item_w = 0;
        int item_h = 0;
        if (is_row) {
            item_w = msz;
            item_h = (do_stretch && content_cross_explicit >= 0) ? content_cross_explicit : 0;
        } else {
            item_w = cross_avail;
            item_h = msz;
        }

        const CellPos tmp_origin = is_row ? CellPos{main_cursor, content_origin.row}
                                          : CellPos{content_origin.col, main_cursor};

        Box child_box = layout_node(*item.sn, tmp_origin, item_w > 0 ? item_w : cross_avail,
                                    item_h > 0 ? item_h : avail_h);

        if (is_row && msz > 0) {
            child_box.border_box.size.cols = msz;
            child_box.content_box.size.cols = std::max(
                0, msz - (child_box.border_box.size.cols - child_box.content_box.size.cols));
        }
        if (!is_row && msz > 0) {
            child_box.border_box.size.rows = msz;
        }

        // Use the actual laid-out size when msz=0 (intrinsic-sized items).
        const int actual_main =
            is_row ? child_box.border_box.size.cols : child_box.border_box.size.rows;
        const int item_advance = (msz > 0) ? msz : actual_main;

        children.push_back(std::move(child_box));
        main_cursor += item_advance + gap_main + jc_between;
    }

    // ── Cross-axis sizing and positioning (align-items) ───────────────────────

    int max_cross = 0;
    for (const auto& cb : children) {
        max_cross = std::max(max_cross, is_row ? cb.border_box.size.rows : cb.border_box.size.cols);
    }
    const int content_cross = (content_cross_explicit >= 0) ? content_cross_explicit : max_cross;

    apply_cross_align(children, st.align_items, is_row, content_cross);

    // ── Build container box ───────────────────────────────────────────────────

    // For column, recompute used height from actual child sizes (items may have been intrinsic).
    int actual_used_main = total_gaps;
    if (!is_row) {
        for (const auto& cb : children) {
            actual_used_main += cb.border_box.size.rows;
        }
    }

    // Text browser: always content-driven; explicit height ignored (see above).
    const int content_h = is_row ? content_cross : actual_used_main;

    Box box;
    box.node = &sn;
    box.border_box = {border_origin,
                      {brd.left + pad.left + content_w + pad.right + brd.right,
                       brd.top + pad.top + content_h + pad.bottom + brd.bottom}};
    box.content_box = {content_origin, {content_w, content_h}};
    box.children = std::move(children);
    return box;
}

}  // namespace

Box layout(const style::StyledNode& root, Viewport vp) {
    Box root_box = layout_block(root, {0, 0}, vp.cols, vp.rows);
    // Width auto already stretches to the viewport (see layout_block); height
    // auto shrink-wraps to content, which would leave the rest of a short
    // page unpainted. Stretch the root box to the full viewport so render
    // has a background to paint over.
    root_box.border_box.size.rows = std::max(root_box.border_box.size.rows, vp.rows);
    return root_box;
}

}  // namespace tvshow::layout
