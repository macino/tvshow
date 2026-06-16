#pragma once

#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"
#include "tvshow/types.hpp"

#include <string_view>
#include <vector>

namespace tvshow::layout {

// One painted character of inline content, with the style of the element it
// came from (text color/weight/etc. can differ per inline span). `href` is
// the target of the nearest enclosing `<a href>`, or empty if the character
// isn't part of a link (SPEC §12.1 focusable elements).
struct InlineToken {
    char32_t cp = 0;
    const style::ComputedStyle* style = nullptr;
    std::string_view href;
};

using InlineLine = std::vector<InlineToken>;

// Flattens the text + inline descendants of `sn` into line boxes that fit
// `content_w` columns, honoring `sn.style.white_space` (SPEC §10.2):
//   - Normal: whitespace runs collapse to a single space; lines break at
//     word boundaries, falling back to a hard character break for any single
//     word wider than `content_w`.
//   - Pre: whitespace is preserved verbatim; lines break only on '\n'.
//   - Nowrap: whitespace collapses like Normal, but no wrapping occurs — the
//     content is a single line that may overflow `content_w`.
// Both the layout stage (to count rows) and the render stage (to place
// characters) call this, so wrapping decisions are identical by
// construction.
std::vector<InlineLine> break_inline(const style::StyledNode& sn, int content_w);

// One token positioned at an absolute grid cell, honoring `sn.style.text_align`.
struct PlacedToken {
    Point pos;
    InlineToken token;
};

// Runs break_inline(sn, content_box.size.cols) and assigns each surviving
// token its absolute cell position, applying text-align and clipping to
// content_box. Render (to paint characters) and layout::collect_links (to
// find focusable link rects) both call this, so token positions agree by
// construction.
std::vector<PlacedToken> place_inline(const style::StyledNode& sn, CellRect content_box);

}  // namespace tvshow::layout
