#include "tvshow/app/browser_view.hpp"

#define Uses_TDrawBuffer
#include "tvshow/paint/paint.hpp"
#include "tvshow/render/chargrid.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <utility>

namespace tvshow::app {

BrowserView::BrowserView(const TRect& bounds, render::CharGrid grid)
    : TView(bounds), grid_(std::move(grid)) {
    growMode = gfGrowHiX | gfGrowHiY;
}

void BrowserView::draw() {
    TDrawBuffer buf;
    const int rows = std::min(size.y, grid_.rows());
    for (int row = 0; row < rows; ++row) {
        paint::draw_row(grid_, row, buf);
        writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
    }
}

}  // namespace tvshow::app
