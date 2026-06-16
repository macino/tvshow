#pragma once

#include "tvshow/layout/box.hpp"
#include "tvshow/render/chargrid.hpp"

namespace tvshow::render {

// Paint a Box tree into a CharGrid. Pure function: no I/O, no tvision.
// The grid is sized to root.border_box (the viewport rect produced by layout).
[[nodiscard]] CharGrid render(const layout::Box& root);

}  // namespace tvshow::render
