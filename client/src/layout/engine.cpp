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
#include <unordered_map>
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

// ── Table column width computation ───────────────────────────────────────────

int measure_text_width(const style::StyledNode& sn, int avail_w) {
    const auto lines = break_inline(sn, avail_w);
    int max_w = 0;
    for (const auto& line : lines) {
        max_w = std::max(max_w, static_cast<int>(line.size()));
    }
    return max_w;
}

// Reads an HTML attribute (colspan/rowspan) as a span count: absent, zero,
// negative, or unparseable all mean "1" (no span) per HTML5 rules.
int cell_span_attr(const dom::Node& node, std::string_view attr_name) noexcept {
    const int v = parse_attr_int(node.attr(attr_name));
    return v > 1 ? v : 1;
}

// Per-table column layout (SPEC Q-27): column widths, and for each <tr>
// StyledNode, the starting grid-column index of each of its td/th cells
// (in document order) so multi-column-spanning cells and rowspan-occupied
// slots from earlier rows don't throw off later cells' alignment.
struct TableGrid {
    std::vector<int> col_widths;
    std::unordered_map<const style::StyledNode*, std::vector<int>> cell_cols;
    // True if any cell has colspan>1 or rowspan>1. td/th have flex-grow:1 in
    // the UA stylesheet, which independently stretches each row's items to
    // fill that row's free space -- harmless when every row has the same
    // item count, but once spans make item counts diverge between rows,
    // per-row growth diverges too and columns drift out of alignment beyond
    // what the base col_widths (and lead_gap, in collect_flex_items) fix.
    // layout_flex uses this to skip further growth for spanned tables,
    // keeping every column at its exact computed width instead.
    bool has_span = false;
};

// Computes column widths and the row->cell-start-column map for a <table>.
// Walks thead/tbody/tfoot wrappers or direct <tr> children. Cells are placed
// into grid columns left-to-right, skipping columns still occupied by a
// rowspan from an earlier row (classic HTML table column-assignment
// algorithm); a colspanning cell's content width is divided evenly across
// the columns it spans (there's no way to know how much belongs to which).
TableGrid compute_table_grid(const style::StyledNode& table_sn, int content_w) {
    TableGrid grid;
    // rows_occupied_after[col] = how many rows AFTER the row currently being
    // processed are still covered by an earlier row's rowspan.
    std::vector<int> rows_occupied_after;

    for (const auto& section_or_row : table_sn.children) {
        if (section_or_row.node == nullptr) { continue; }
        const auto& rows = (section_or_row.node->tag == "tr")
                               ? std::vector<const style::StyledNode*>{&section_or_row}
                               : [&]() {
                                     std::vector<const style::StyledNode*> out;
                                     for (const auto& c : section_or_row.children) {
                                         if (c.node != nullptr && c.node->tag == "tr") {
                                             out.push_back(&c);
                                         }
                                     }
                                     return out;
                                 }();
        for (const auto* row : rows) {
            std::vector<int>& cell_cols = grid.cell_cols[row];
            std::vector<bool> touched;
            int col_idx = 0;
            for (const auto& cell : row->children) {
                if (cell.node == nullptr || cell.node->kind != dom::NodeKind::Element) {
                    continue;
                }
                if (cell.node->tag != "td" && cell.node->tag != "th") { continue; }

                while (col_idx < static_cast<int>(rows_occupied_after.size()) &&
                      rows_occupied_after[static_cast<size_t>(col_idx)] > 0) {
                    ++col_idx;
                }
                cell_cols.push_back(col_idx);

                const int colspan = cell_span_attr(*cell.node, "colspan");
                const int rowspan = cell_span_attr(*cell.node, "rowspan");
                if (colspan > 1 || rowspan > 1) { grid.has_span = true; }
                const EdgePx cpad = compute_padding(cell.style.padding, content_w, 0);
                const int text_w = measure_text_width(cell, content_w);
                const int cell_w = cpad.left + text_w + cpad.right;
                const int per_col_w = (cell_w + colspan - 1) / colspan;  // ceil-divide

                const auto need = static_cast<size_t>(col_idx + colspan);
                if (grid.col_widths.size() < need) { grid.col_widths.resize(need, 0); }
                if (rows_occupied_after.size() < need) { rows_occupied_after.resize(need, 0); }
                if (touched.size() < need) { touched.resize(need, false); }

                for (int c = col_idx; c < col_idx + colspan; ++c) {
                    grid.col_widths[static_cast<size_t>(c)] =
                        std::max(grid.col_widths[static_cast<size_t>(c)], per_col_w);
                    rows_occupied_after[static_cast<size_t>(c)] =
                        std::max(rows_occupied_after[static_cast<size_t>(c)], rowspan - 1);
                    touched[static_cast<size_t>(c)] = true;
                }
                col_idx += colspan;
            }
            // A row boundary has passed: columns occupied by an earlier
            // row's rowspan (not touched by this row's own cells) have one
            // fewer row of occupancy remaining.
            for (size_t c = 0; c < rows_occupied_after.size(); ++c) {
                if ((c >= touched.size() || !touched[c]) && rows_occupied_after[c] > 0) {
                    --rows_occupied_after[c];
                }
            }
        }
    }
    return grid;
}

// Thread-local table grid context for layout_flex to use.
// Set by layout_block when processing <table>, cleared after.
thread_local const TableGrid* g_table_grid = nullptr;

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

// Flattens a <table>'s already-laid-out children into a list of <tr> row
// boxes, in document order, descending one level into thead/tbody/tfoot
// section boxes (rows are direct table children only when no section wraps
// them).
void collect_row_boxes(std::vector<Box>& table_children, std::vector<Box*>& out) {
    for (auto& b : table_children) {
        if (b.node != nullptr && b.node->node != nullptr && b.node->node->tag == "tr") {
            out.push_back(&b);
        } else {
            for (auto& rb : b.children) {
                if (rb.node != nullptr && rb.node->node != nullptr && rb.node->node->tag == "tr") {
                    out.push_back(&rb);
                }
            }
        }
    }
}

// Extends each rowspan>1 cell's box height to cover the rows it spans (SPEC
// Q-27). Cells were laid out with their own row's intrinsic height (row
// heights aren't known ahead of time in this row-by-row layout); this
// widens the box downward now that every row's actual height is known.
// The spanned column in later rows is already left empty by the col_idx
// occupancy skip in compute_table_grid, so the extended box doesn't overlap
// any sibling cell.
void apply_table_rowspans(std::vector<Box>& table_children) {
    std::vector<Box*> rows;
    collect_row_boxes(table_children, rows);
    for (size_t ri = 0; ri < rows.size(); ++ri) {
        for (auto& cell : rows[ri]->children) {
            if (cell.node == nullptr || cell.node->node == nullptr) { continue; }
            const int rowspan = cell_span_attr(*cell.node->node, "rowspan");
            if (rowspan <= 1) { continue; }
            const int extra_rows =
                std::min(rowspan - 1, static_cast<int>(rows.size() - ri - 1));
            int extra_h = 0;
            for (int k = 1; k <= extra_rows; ++k) {
                extra_h += rows[ri + static_cast<size_t>(k)]->border_box.size.rows;
            }
            cell.border_box.size.rows += extra_h;
            cell.content_box.size.rows += extra_h;
        }
    }
}

// ── position:fixed / position:sticky overlays (SPEC Q-29) ─────────────────────

// Viewport size for the current top-level layout() call and the overlay
// accumulator it's collecting into. Set once per layout() call (unlike
// g_table_grid, there's no nesting to save/restore -- only one viewport
// exists per layout pass).
thread_local Viewport g_overlay_vp{0, 0};
thread_local std::vector<OverlayBox>* g_overlays = nullptr;

// Resolves an out-of-flow element's viewport-relative pinned origin from its
// own size and top/left/right/bottom offsets. An axis with both offsets
// auto defaults to the start edge (0) -- fixed/sticky elements almost
// always specify at least one offset in practice (e.g. `top: 0`).
CellPos resolve_overlay_origin(const style::ComputedStyle& st, Size box_size, int vp_cols,
                               int vp_rows) {
    int x = 0;
    if (!st.left_offset.is_auto) {
        x = resolve_h(st.left_offset, vp_cols);
    } else if (!st.right_offset.is_auto) {
        x = vp_cols - resolve_h(st.right_offset, vp_cols) - box_size.cols;
    }
    int y = 0;
    if (!st.top.is_auto) {
        y = resolve_v(st.top, vp_rows);
    } else if (!st.bottom.is_auto) {
        y = vp_rows - resolve_v(st.bottom, vp_rows) - box_size.rows;
    }
    return {x, y};
}

// Builds the OverlayBox for a fixed/sticky child: laid out at local origin
// (0,0) so render::render(overlay.box) yields exactly its own content,
// ready to be blitted onto the viewport elsewhere by the view layer.
// static_doc_row is meaningless for Fixed (pass 0); for Sticky it's the row
// the element occupies in normal flow, needed by the view to decide whether
// the in-flow copy is already visible or the overlay should paint instead.
OverlayBox make_overlay_box(const style::StyledNode& child, int content_w, int avail_h,
                            int static_doc_row) {
    Box local_box = layout_node(child, {0, 0}, content_w, avail_h);
    OverlayBox ov;
    ov.pinned_origin = resolve_overlay_origin(child.style, local_box.border_box.size,
                                              g_overlay_vp.cols, g_overlay_vp.rows);
    ov.kind = (child.style.position == style::Position::Fixed) ? OverlayKind::Fixed
                                                                : OverlayKind::Sticky;
    ov.static_doc_row = static_doc_row;
    ov.box = std::move(local_box);
    return ov;
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

    // Table column alignment: compute the column grid for <table> elements.
    TableGrid table_grid;
    const bool is_table = sn.node != nullptr && sn.node->tag == "table";
    const TableGrid* prev_grid = g_table_grid;
    if (is_table) {
        table_grid = compute_table_grid(sn, content_w);
        // table_grid outlives all recursive layout calls below; restored before return.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
        g_table_grid = &table_grid;
#pragma GCC diagnostic pop
    }

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
        // Fixed: never in flow, pinned to the viewport (SPEC Q-29). Only
        // handled here (block-level children) -- a fixed/sticky item
        // directly inside a flex container is still dropped, as before;
        // that's a rare pattern (nav bars/headers are block-level) and
        // keeping this scoped avoids reworking layout_flex's item-sizing
        // pass to also carry overlay boxes.
        if (child.style.position == style::Position::Fixed) {
            if (g_overlays != nullptr) {
                g_overlays->push_back(make_overlay_box(child, content_w, avail_h, 0));
            }
            continue;
        }
        // Sticky: stays in flow (its own row is reserved like any block),
        // but also gets an overlay entry so the view can pin it once
        // scroll_row_ carries its normal-flow row above pinned_origin.row.
        if (child.style.position == style::Position::Sticky) {
            const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
            Box child_box = layout_node(child, {content_origin.col, y}, content_w, avail_h);
            const int static_row = child_box.border_box.origin.row;
            y += cmar.top + child_box.border_box.size.rows + cmar.bottom;
            children.push_back(std::move(child_box));
            if (g_overlays != nullptr) {
                g_overlays->push_back(make_overlay_box(child, content_w, avail_h, static_row));
            }
            continue;
        }
        if (disp == style::Display::Block || disp == style::Display::Flex ||
            disp == style::Display::InlineBlock) {
            const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
            Box child_box = layout_node(child, {content_origin.col, y}, content_w, avail_h);
            // position:absolute is removed from normal flow (SPEC Q-25): it still
            // gets an intrinsic size and a static fallback position (used when
            // top/left/right/bottom are all auto), but doesn't reserve flow space
            // for following siblings. apply_position_offsets() re-anchors it to
            // its nearest positioned ancestor afterwards.
            if (child.style.position != style::Position::Absolute) {
                y += cmar.top + child_box.border_box.size.rows + cmar.bottom;
            }
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

    if (is_table) {
        apply_table_rowspans(children);
        g_table_grid = prev_grid;
    }

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
    // Table rows only: blank space to insert before this item for grid
    // columns skipped because an earlier row's rowspan still occupies them.
    int lead_gap = 0;
};

// Collect flex items from a container's children, skipping display:none and
// hidden form controls. Returns items in source order.
// gap_main only matters for table rows: it's added between the columns a
// colspanning cell covers.
std::vector<FlexItem> collect_flex_items(const style::StyledNode& sn, bool is_row, int content_main,
                                         int avail_cross, int gap_main) {
    // Table row: use the pre-computed column grid if available.
    const bool is_tr = sn.node != nullptr && sn.node->tag == "tr" && g_table_grid != nullptr;
    const std::vector<int>* row_cols = nullptr;
    if (is_tr) {
        const auto it = g_table_grid->cell_cols.find(&sn);
        if (it != g_table_grid->cell_cols.end()) { row_cols = &it->second; }
    }

    std::vector<FlexItem> items;
    int cell_i = 0;    // index into row_cols: the nth td/th cell in this row
    int next_col = 0;  // grid column the next item is expected to start at
    for (const auto& child : sn.children) {
        if (child.node == nullptr || child.node->kind != dom::NodeKind::Element) {
            continue;
        }
        if (child.style.display == style::Display::None) {
            continue;
        }
        if (child.style.position == style::Position::Fixed ||
            child.style.position == style::Position::Sticky) {
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
        int lead_gap = 0;
        if (row_cols != nullptr && is_row && cell_i < static_cast<int>(row_cols->size())) {
            // Sum the widths of every column this cell's colspan covers,
            // plus the gaps between them.
            const int start_col = (*row_cols)[static_cast<size_t>(cell_i)];
            const int colspan = cell_span_attr(*child.node, "colspan");
            const int end_col = std::min(start_col + colspan,
                                         static_cast<int>(g_table_grid->col_widths.size()));
            for (int c = start_col; c < end_col; ++c) {
                base += g_table_grid->col_widths[static_cast<size_t>(c)];
            }
            base += gap_main * std::max(0, end_col - start_col - 1);
            // Columns between next_col and start_col are occupied by an
            // earlier row's rowspan -- no item sits there, so insert blank
            // space instead, otherwise this item would render flush against
            // the previous one instead of under its real grid column.
            if (start_col > next_col) {
                for (int c = next_col; c < start_col; ++c) {
                    lead_gap += g_table_grid->col_widths[static_cast<size_t>(c)];
                }
                lead_gap += gap_main * (start_col - next_col);
            }
            next_col = end_col;
            ++cell_i;
        } else {
            const auto& fb = child.style.flex_basis;
            if (!fb.is_auto) {
                base = is_row ? resolve_h(fb, content_main) : resolve_v(fb, avail_cross);
            } else if (is_row && !child.style.width.is_auto) {
                base = resolve_h(child.style.width, content_main);
            } else if (!is_row && !child.style.height.is_auto) {
                base = resolve_v(child.style.height, avail_cross);
            }
        }
        items.push_back({&child, base, child.style.flex_grow, child.style.flex_shrink, lead_gap});
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
        collect_flex_items(sn, is_row, content_main, is_row ? avail_h : content_w, gap_main);
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
    // Spanned tables keep every column at its exact grid width (see
    // TableGrid::has_span) -- growing items further would drift columns out
    // of alignment whenever a row's item count differs from the table's
    // real column count.
    const bool is_spanned_table_row =
        sn.node != nullptr && sn.node->tag == "tr" && g_table_grid != nullptr && g_table_grid->has_span;
    if (!is_spanned_table_row) {
        distribute_flex_space(main_sizes, items, main_available - total_base - total_gaps);
    }

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
        main_cursor += item.lead_gap;

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

// Apply position offsets to a laid-out box (SPEC §20 Q-25).
// relative: offset from normal-flow position (relative to the immediate parent).
// absolute: offset from the nearest ancestor with position != static (`anc_*`),
// or the viewport when no ancestor is positioned. Any position != static
// establishes a new containing block for absolutely-positioned descendants.
void apply_position_offsets(Box& box, int parent_w, int parent_h, CellPos anc_origin, int anc_w,
                            int anc_h) {
    if (box.node == nullptr) { return; }
    const auto& st = box.node->style;
    if (st.position == style::Position::Relative) {
        int dx = 0;
        int dy = 0;
        if (!st.left_offset.is_auto) {
            dx = resolve_h(st.left_offset, parent_w);
        } else if (!st.right_offset.is_auto) {
            dx = -resolve_h(st.right_offset, parent_w);
        }
        if (!st.top.is_auto) {
            dy = resolve_v(st.top, parent_h);
        } else if (!st.bottom.is_auto) {
            dy = -resolve_v(st.bottom, parent_h);
        }
        box.border_box.origin.col += dx;
        box.border_box.origin.row += dy;
        box.content_box.origin.col += dx;
        box.content_box.origin.row += dy;
    } else if (st.position == style::Position::Absolute) {
        if (!st.left_offset.is_auto) {
            const int x = anc_origin.col + resolve_h(st.left_offset, anc_w);
            const int dx = x - box.border_box.origin.col;
            box.border_box.origin.col += dx;
            box.content_box.origin.col += dx;
        } else if (!st.right_offset.is_auto) {
            const int x = anc_origin.col + anc_w - resolve_h(st.right_offset, anc_w) -
                          box.border_box.size.cols;
            const int dx = x - box.border_box.origin.col;
            box.border_box.origin.col += dx;
            box.content_box.origin.col += dx;
        }
        if (!st.top.is_auto) {
            const int y = anc_origin.row + resolve_v(st.top, anc_h);
            const int dy = y - box.border_box.origin.row;
            box.border_box.origin.row += dy;
            box.content_box.origin.row += dy;
        } else if (!st.bottom.is_auto) {
            const int y = anc_origin.row + anc_h - resolve_v(st.bottom, anc_h) -
                          box.border_box.size.rows;
            const int dy = y - box.border_box.origin.row;
            box.border_box.origin.row += dy;
            box.content_box.origin.row += dy;
        }
    }
    const bool establishes_cb = st.position != style::Position::Static;
    const CellPos next_anc_origin = establishes_cb ? box.content_box.origin : anc_origin;
    const int next_anc_w = establishes_cb ? box.content_box.size.cols : anc_w;
    const int next_anc_h = establishes_cb ? box.content_box.size.rows : anc_h;
    for (auto& child : box.children) {
        apply_position_offsets(child, box.content_box.size.cols, box.content_box.size.rows,
                               next_anc_origin, next_anc_w, next_anc_h);
    }
}

Box layout(const style::StyledNode& root, Viewport vp) {
    std::vector<OverlayBox> overlays;
    g_overlay_vp = vp;
    g_overlays = &overlays;

    Box root_box = layout_block(root, {0, 0}, vp.cols, vp.rows);
    apply_position_offsets(root_box, vp.cols, vp.rows, {0, 0}, vp.cols, vp.rows);
    root_box.border_box.size.rows = std::max(root_box.border_box.size.rows, vp.rows);

    g_overlays = nullptr;
    root_box.overlays = std::move(overlays);
    return root_box;
}

// NOLINTNEXTLINE(misc-no-recursion)
const Box* hit_test(const Box& root, Point pt) {
    if (!root.border_box.contains(pt)) {
        return nullptr;
    }
    for (auto it = root.children.rbegin(); it != root.children.rend(); ++it) {
        const Box* found = hit_test(*it, pt);
        if (found != nullptr) {
            return found;
        }
    }
    return &root;
}

}  // namespace tvshow::layout
