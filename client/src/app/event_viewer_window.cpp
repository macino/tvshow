#include "tvshow/app/event_viewer_window.hpp"

#define Uses_TDialog
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TListViewer
#define Uses_TScrollBar
#include <tvision/tv.h>

#include <algorithm>
#include <cstring>
#include <format>

namespace tvshow::app {

class EventLogViewer : public TListViewer {
public:
    EventLogViewer(const TRect& bounds, TScrollBar* sb, const std::vector<std::string>* lines)
        : TListViewer(bounds, 1, nullptr, sb), lines_(lines) {}

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void getText(char* dest, short item, short maxLen) override {
        const auto idx = static_cast<size_t>(item);
        if (idx < lines_->size()) {
            std::strncpy(dest, (*lines_)[idx].c_str(), static_cast<size_t>(maxLen));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dest[maxLen] = '\0';
    }

    void sync() {
        setRange(static_cast<short>(lines_->size()));
        if (range > 0) { focusItem(static_cast<short>(range - 1)); }
        drawView();
    }

private:
    const std::vector<std::string>* lines_;
};

EventViewerWindow::EventViewerWindow(const TRect& bounds)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Event Viewer", wnNoNumber) {
    options |= ofSelectable;
    eventMask = 0xFFFF;  // capture every event kind this window is offered, not just the default set

    const TRect inner = getExtent().grow(-1, -1);
    auto* sb = new TScrollBar(TRect{inner.b.x - 1, inner.a.y, inner.b.x, inner.b.y});
    insert(sb);
    auto* viewer = new EventLogViewer(TRect{inner.a.x, inner.a.y, inner.b.x - 1, inner.b.y}, sb,
                                      &lines_);
    insert(viewer);
    viewer_ = viewer;
}

void EventViewerWindow::log_event(const TEvent& event) {
    std::string line;
    switch (event.what) {
    case evMouseDown:
    case evMouseUp:
    case evMouseMove:
        line = std::format("mouse: where=({},{}) buttons={:#04x} wheel={:#04x}",
                           event.mouse.where.x, event.mouse.where.y, event.mouse.buttons,
                           event.mouse.wheel);
        break;
    case evKeyDown:
        line = std::format("key: code={:#06x} charCode={:#04x} scanCode={:#04x}",
                           event.keyDown.keyCode, event.keyDown.charScan.charCode,
                           event.keyDown.charScan.scanCode);
        break;
    default:
        return;  // not one of the kinds this demo window narrates
    }
    lines_.push_back(std::move(line));
    if (lines_.size() > 200) { lines_.erase(lines_.begin()); }  // cap, same spirit as RequestLog
    if (viewer_ != nullptr) { viewer_->sync(); }
}

void EventViewerWindow::handleEvent(TEvent& event) {
    log_event(event);
    TWindow::handleEvent(event);
}

void EventViewerWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    // The scrollbar's column position is computed once at construction and
    // doesn't move on resize (adr-native-demo-windows' documented v1
    // limitation) -- shrinking too far pushes it past the new right edge.
    // A generous floor here (this window's content has no fixed minimum of
    // its own, unlike Calendar/Calculator/Puzzle's absolutely-positioned
    // controls) keeps that from becoming visible.
    min.x = std::max(min.x, 30);
    min.y = std::max(min.y, 10);
}

TPalette& EventViewerWindow::getPalette() const {
    // cpGrayDialog, not the default (short) TWindow palette -- see
    // CalculatorWindow::getPalette() for why. Without this, TListViewer's
    // and TScrollBar's colors fall through to an unstyled default that
    // happens to be the same red as the list background -- the scrollbar
    // wasn't overlapped, it was invisible: same red on red.
    static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
