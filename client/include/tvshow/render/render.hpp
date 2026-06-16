#pragma once

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace tvshow::dom {
struct Node;
}

namespace tvshow::render {

// Mutable form control state layered on top of DOM attr initial values.
// Keyed by dom::Node* (stable for the lifetime of Page).
struct FormValues {
    std::unordered_map<const dom::Node*, std::string> text;  // text/password/textarea/select
    std::unordered_map<const dom::Node*, bool> checked;      // checkbox/radio
};

// Paint a Box tree into a CharGrid. Pure function: no I/O, no tvision.
// The grid is sized to root.border_box (the viewport rect produced by layout).
// fv overrides DOM attr initial values for form controls.
[[nodiscard]] CharGrid render(const layout::Box& root, const FormValues& fv = {});

// Inverts fg/bg for every cell in `spans` (SPEC §12.1 focus highlight for the
// currently-focused link). Cells outside the grid bounds are skipped, so
// callers don't need to pre-clip spans.
void apply_focus(CharGrid& grid, const std::vector<layout::CellRect>& spans);

}  // namespace tvshow::render
