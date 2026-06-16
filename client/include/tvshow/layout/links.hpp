#pragma once

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"

#include <string_view>
#include <vector>

namespace tvshow::layout {

// One focusable `<a href>` occurrence (SPEC §12.1). `spans` holds one CellRect
// per visual row the link's text occupies, in reading order, so a link
// wrapped across lines still gets one Link with multiple spans.
struct Link {
    std::string_view href;
    std::vector<CellRect> spans;
};

// Walks the box tree and groups contiguous same-link tokens (as placed by
// place_inline) into Link entries, in reading order. Render (to invert
// fg/bg on the focused link) and BrowserView (to drive Tab/Shift-Tab focus
// cycling and Enter navigation) both call this, so link identity and
// position agree by construction.
std::vector<Link> collect_links(const Box& root);

}  // namespace tvshow::layout
