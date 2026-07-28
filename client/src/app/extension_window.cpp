#include "tvshow/app/extension_window.hpp"

#define Uses_TEvent
#define Uses_TKeys
#define Uses_TListViewer
#define Uses_TScrollBar
#include <tvision/tv.h>

#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

namespace tvshow::app {

namespace {

constexpr int kInputHeight = 1;

}  // namespace

// Read-only scrollback list backed directly by ExtensionWindow::lines_
// (mirrors BrowserView's OptionListViewer pattern: TListViewer + a pointer
// to an externally-owned vector, no data duplication).
class ScrollbackViewer : public TListViewer {
public:
    ScrollbackViewer(const TRect& bounds, TScrollBar* sb, const std::vector<std::string>* lines)
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
        if (range > 0) { focusItem(static_cast<short>(range - 1)); }  // auto-scroll to bottom
        drawView();
    }

private:
    const std::vector<std::string>* lines_;
};

ExtensionWindow::ExtensionWindow(const TRect& bounds, const char* win_title,
                                 std::vector<std::string> argv)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, win_title, wnNoNumber) {
    process_ = std::make_unique<ExtensionProcess>(std::move(argv));

    const TRect inner = getExtent().grow(-1, -1);
    const TRect input_rect{inner.a.x, inner.b.y - kInputHeight, inner.b.x, inner.b.y};
    input_ = new TInputLine(input_rect, 255);
    insert(input_);

    const TRect sb_rect{inner.b.x - 1, inner.a.y, inner.b.x, inner.b.y - kInputHeight};
    auto* sb = new TScrollBar(sb_rect);
    insert(sb);

    const TRect view_rect{inner.a.x, inner.a.y, inner.b.x - 1, inner.b.y - kInputHeight};
    auto* viewer = new ScrollbackViewer(view_rect, sb, &lines_);
    insert(viewer);
    viewer_ = viewer;

    if (process_->alive()) {
        append_line("[extension started]");
    } else {
        append_line("[failed to start extension -- check the configured command]");
    }

    input_->select();
}

void ExtensionWindow::append_line(std::string line) {
    lines_.push_back(std::move(line));
    if (viewer_ != nullptr) { viewer_->sync(); }
}

void ExtensionWindow::poll() {
    if (process_ == nullptr) {
        return;
    }
    std::string chunk = process_->read_available();
    if (!chunk.empty()) {
        std::istringstream in(chunk);
        std::string line;
        while (std::getline(in, line)) { append_line(line); }
    }
    if (!process_->alive() && !exited_notice_shown_) {
        exited_notice_shown_ = true;
        append_line("[extension exited]");
    }
}

void ExtensionWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);

    if (event.what == evKeyDown &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.keyDown.keyCode == kbEnter &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        input_ != nullptr) {
        std::array<char, 256> buf{};
        input_->getData(buf.data());
        const std::string text(buf.data());
        if (!text.empty() && process_ != nullptr) {
            append_line("> " + text);
            process_->write_line(text);
        }
        buf.fill('\0');
        input_->setData(buf.data());
        clearEvent(event);
    }
}

void ExtensionWindow::reposition(const TRect& inner) {
    const TRect input_rect{inner.a.x, inner.b.y - kInputHeight, inner.b.x, inner.b.y};
    if (input_ != nullptr) { input_->changeBounds(input_rect); }
    const TRect view_rect{inner.a.x, inner.a.y, inner.b.x - 1, inner.b.y - kInputHeight};
    if (viewer_ != nullptr) { viewer_->changeBounds(view_rect); }
}

void ExtensionWindow::changeBounds(const TRect& bounds) {
    TWindow::changeBounds(bounds);
    reposition(getExtent().grow(-1, -1));
}

}  // namespace tvshow::app
