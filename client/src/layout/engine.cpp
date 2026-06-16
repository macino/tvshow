#include "tvshow/layout/engine.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"

#include <algorithm>
#include <cmath>
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

// NOLINTNEXTLINE(misc-no-recursion)
int count_inline_chars(const style::StyledNode& sn) noexcept {
    int total = 0;
    for (const auto& child : sn.children) {
        if (child.node == nullptr) {
            continue;
        }
        if (child.node->kind == dom::NodeKind::Text) {
            total += static_cast<int>(child.node->text.size());
        } else if (child.style.display != style::Display::None &&
                   child.style.display != style::Display::Block &&
                   child.style.display != style::Display::Flex) {
            total += count_inline_chars(child);
        }
    }
    return total;
}

int inline_rows(const style::StyledNode& sn, int content_w) noexcept {
    if (content_w <= 0) {
        return 0;
    }
    const int chars = count_inline_chars(sn);
    if (chars == 0) {
        return 0;
    }
    return (chars + content_w - 1) / content_w;
}

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
    const int content_w = st.width.is_auto ? std::max(0, avail_w - chrome_h)
                                           : resolve_h(st.width, avail_w);

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
        const auto disp = child.style.display;
        if (disp == style::Display::None) {
            continue;
        }
        if (disp == style::Display::Block || disp == style::Display::Flex ||
            disp == style::Display::InlineBlock) {
            const EdgePx cmar = compute_margin(child.style.margin, content_w, avail_h);
            y += cmar.top;
            Box child_box = layout_block(child, {content_origin.col, y}, content_w, avail_h);
            y += child_box.border_box.size.rows + cmar.bottom;
            children.push_back(std::move(child_box));
        }
    }

    if (has_inline_content(sn)) {
        y += inline_rows(sn, content_w);
    }

    const int content_h = st.height.is_auto ? std::max(0, y - content_origin.row)
                                            : resolve_v(st.height, avail_h);

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
    return layout_block(root, {0, 0}, vp.cols, vp.rows);
}

}  // namespace tvshow::layout
