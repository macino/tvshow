#pragma once

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"

namespace tvshow::layout {

// Lay out a styled tree and produce a Box tree.
// root: the root StyledNode (typically <html>).
// vp: terminal viewport in character cells.
// Returns a Box tree rooted at root; display:none nodes are omitted.
[[nodiscard]] Box layout(const style::StyledNode& root, Viewport vp);

}  // namespace tvshow::layout
