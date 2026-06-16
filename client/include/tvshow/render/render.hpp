#pragma once

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"

#include <vector>

namespace tvshow::render {

// Paint a Box tree into a CharGrid. Pure function: no I/O, no tvision.
// The grid is sized to root.border_box (the viewport rect produced by layout).
[[nodiscard]] CharGrid render(const layout::Box& root);

// Inverts fg/bg for every cell in `spans` (SPEC §12.1 focus highlight for the
// currently-focused link). Cells outside the grid bounds are skipped, so
// callers don't need to pre-clip spans.
void apply_focus(CharGrid& grid, const std::vector<layout::CellRect>& spans);

}  // namespace tvshow::render
