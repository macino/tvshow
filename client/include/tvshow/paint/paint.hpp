#pragma once

#include "tvshow/render/chargrid.hpp"

class TDrawBuffer;

namespace tvshow::paint {

// Paint one row of `grid` into a tvision TDrawBuffer, ready for TView::writeLine.
// Lives outside the pure pipeline (SPEC §3.1) because it depends on tvision
// types, but it does no I/O of its own — the caller owns the TView and the
// actual screen write.
void draw_row(const render::CharGrid& grid, int row, TDrawBuffer& buf);

}  // namespace tvshow::paint
