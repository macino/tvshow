#pragma once

#include "tvshow/images/renderer.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tvshow::dom {
struct Node;
}

namespace tvshow::render {

// Mutable form control state layered on top of DOM attr initial values.
// Keyed by dom::Node* (stable for the lifetime of Page).
struct FormValues {
    std::unordered_map<const dom::Node*, std::string> text;  // text/password/textarea/select/file (full path)
    std::unordered_map<const dom::Node*, bool> checked;      // checkbox/radio
};

// Optional rendering parameters.
struct RenderOpts {
    std::string_view base_url;  // page URL for resolving link hrefs
    const std::unordered_set<std::string>* visited_urls = nullptr;  // resolved absolute URLs
};

// Paint a Box tree into a CharGrid. Pure function: no I/O, no tvision.
// The grid is sized to root.border_box (the viewport rect produced by layout).
// fv overrides DOM attr initial values for form controls.
// img_renderer selects how <img> elements are painted; null (default) uses
// AltTextRenderer. Pass a BrailleRenderer to render decoded image pixels
// (still pure: BrailleRenderer only reads an already-populated ImageCache,
// no I/O happens during render()).
[[nodiscard]] CharGrid render(const layout::Box& root, const FormValues& fv = {},
                              const RenderOpts& opts = {},
                              const images::ImageRenderer* img_renderer = nullptr);

// Inverts fg/bg for every cell in `spans` (SPEC §12.1 focus highlight for the
// currently-focused link). Cells outside the grid bounds are skipped, so
// callers don't need to pre-clip spans.
void apply_focus(CharGrid& grid, const std::vector<layout::CellRect>& spans);

// Returns true if grid has fewer than 20 visible (non-space) cells — used
// to detect pages where author CSS hides all content (JS-dependent pages).
[[nodiscard]] bool is_mostly_blank(const CharGrid& grid) noexcept;

// Paints debug box outlines (magenta) over each box in the tree.
// Corners: ┌┐└┘  edges: ─ │
void apply_debug_overlay(CharGrid& grid, const layout::Box& root);

// Paints a focus-order index (0, 1, 2, ...) at the start of each anchor span,
// in the order given (links then form controls, as BrowserView tracks focus).
// Part of the Ctrl-D debug overlay.
void apply_focus_order_labels(CharGrid& grid, const std::vector<layout::CellRect>& anchors);

// Result of collapse_blank_rows: collapsed grid + row mapping.
// kept_rows[collapsed_row] = original_row in the source grid.
struct CollapseResult {
    CharGrid grid;
    std::vector<int> kept_rows;
};

// Returns a new grid with runs of blank rows (all cells are U' ') collapsed
// to at most `max_consecutive`. Useful for graphics-heavy pages where images
// and unsupported elements leave large empty stretches.
[[nodiscard]] CollapseResult collapse_blank_rows(const CharGrid& grid, int max_consecutive = 2);

}  // namespace tvshow::render
