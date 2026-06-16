#include "tvshow/app/browser_view.hpp"

#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#include "tvshow/app/page.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/paint/paint.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace tvshow::app {

BrowserView::BrowserView(const TRect& bounds, Page page) : TView(bounds), page_(std::move(page)) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;
    eventMask |= evKeyDown;
    links_ = layout::collect_links(page_.box);
    history_.push_back(page_.url);
}

render::CharGrid BrowserView::render_grid() const {
    render::CharGrid grid = render::render(page_.box);
    if (focused_ >= 0 && focused_ < static_cast<int>(links_.size())) {
        render::apply_focus(grid, links_[focused_].spans);
    }
    return grid;
}

void BrowserView::draw() {
    TDrawBuffer buf;
    const render::CharGrid grid = render_grid();
    const int rows = std::min(size.y, grid.rows());
    for (int row = 0; row < rows; ++row) {
        paint::draw_row(grid, row, buf);
        writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
    }
}

void BrowserView::changeBounds(const TRect& bounds) {
    TView::changeBounds(bounds);
    relayout();
    drawView();
}

void BrowserView::relayout() {
    page_.box = layout::layout(*page_.tree, {size.x, size.y});
    links_ = layout::collect_links(page_.box);
    if (focused_ >= static_cast<int>(links_.size())) {
        focused_ = -1;
    }
}

void BrowserView::navigate(const std::string& url, bool push_history) {
    auto page = load_page(url, {size.x, size.y});
    if (!page) {
        return;
    }
    page_ = std::move(*page);
    links_ = layout::collect_links(page_.box);
    focused_ = -1;
    if (push_history) {
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_pos_) + 1,
                       history_.end());
        history_.push_back(url);
        history_pos_ = history_.size() - 1;
    }
    drawView();
}

void BrowserView::focus_next(int direction) {
    if (links_.empty()) {
        return;
    }
    const int n = static_cast<int>(links_.size());
    if (focused_ < 0) {
        focused_ = direction > 0 ? 0 : n - 1;
    } else {
        focused_ = (focused_ + direction + n) % n;
    }
    drawView();
}

void BrowserView::handleEvent(TEvent& event) {
    TView::handleEvent(event);
    if (event.what != evKeyDown) {
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) — TEvent is a tvision union.
    switch (event.keyDown.keyCode) {
    case kbTab:
        focus_next(1);
        clearEvent(event);
        break;
    case kbShiftTab:
        focus_next(-1);
        clearEvent(event);
        break;
    case kbEnter:
        if (focused_ >= 0 && focused_ < static_cast<int>(links_.size())) {
            navigate(util::resolve_url(page_.url, links_[focused_].href), true);
        }
        clearEvent(event);
        break;
    case kbAltLeft:
        if (history_pos_ > 0) {
            --history_pos_;
            navigate(history_[history_pos_], false);
        }
        clearEvent(event);
        break;
    case kbAltRight:
        if (history_pos_ + 1 < history_.size()) {
            ++history_pos_;
            navigate(history_[history_pos_], false);
        }
        clearEvent(event);
        break;
    default:
        break;
    }
}

}  // namespace tvshow::app
