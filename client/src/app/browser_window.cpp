#include "tvshow/app/browser_window.hpp"

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TEvent
#define Uses_TKeys
#define Uses_TScrollBar
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace tvshow::app {

namespace {
constexpr int kBarHeight = 1;
constexpr int kUrlMaxLen = 511;

bool is_navigable(const std::string& url) {
    return util::Url::parse(url).has_value() || url.starts_with("file://");
}
}  // namespace

BrowserWindow::BrowserWindow(const TRect& bounds, AddressBarMode mode, Page page)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, page.url.c_str(), wnNoNumber), mode_(mode) {
    options |= ofTileable;
    const TRect inner = getExtent().grow(-1, -1);

    if (mode_ == AddressBarMode::Persistent) {
        TRect bar_rect = inner;
        bar_rect.b.y = bar_rect.a.y + kBarHeight;
        bar_ = new TInputLine(bar_rect, kUrlMaxLen);
        std::array<char, kUrlMaxLen + 1> buf{};
        std::strncpy(buf.data(), page.url.c_str(), kUrlMaxLen);
        bar_->setData(buf.data());
        insert(bar_);
    }

    const int view_top = inner.a.y + (mode_ == AddressBarMode::Persistent ? kBarHeight : 0);
    const TRect sb_rect{inner.b.x - 1, view_top, inner.b.x, inner.b.y};
    vscroll_ = new TScrollBar(sb_rect);
    insert(vscroll_);

    const TRect view_rect{inner.a.x, view_top, inner.b.x - 1, inner.b.y};
    view_ = new BrowserView(view_rect, std::move(page));
    view_->set_vscroll(vscroll_);
    insert(view_);
}

void BrowserWindow::reposition(const TRect& inner) {
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr) {
        TRect bar_rect = inner;
        bar_rect.b.y = bar_rect.a.y + kBarHeight;
        bar_->changeBounds(bar_rect);
    }
    const int view_top = inner.a.y + (mode_ == AddressBarMode::Persistent ? kBarHeight : 0);
    if (vscroll_ != nullptr) {
        const TRect sb_rect{inner.b.x - 1, view_top, inner.b.x, inner.b.y};
        vscroll_->changeBounds(sb_rect);
    }
    const TRect view_rect{inner.a.x, view_top, inner.b.x - 1, inner.b.y};
    view_->changeBounds(view_rect);
}

void BrowserWindow::changeBounds(const TRect& bounds) {
    TWindow::changeBounds(bounds);
    reposition(getExtent().grow(-1, -1));
}

void BrowserWindow::focus_address_bar() {
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr) {
        bar_->select();
        bar_->selectAll(true);
    }
}

void BrowserWindow::navigate(std::string_view url) {
    const std::string s(url);
    if (is_navigable(s)) {
        view_->navigate_to(s);
    }
}

void BrowserWindow::handleEvent(TEvent& event) {
    // Scrollbar changed: update view scroll position.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    if (event.what == evBroadcast && event.message.command == cmScrollBarChanged &&
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        event.message.infoPtr == vscroll_ && vscroll_ != nullptr) {
        view_->scroll_to(vscroll_->value);
        clearEvent(event);
        return;
    }
    // Persistent bar: Enter while bar_ is focused → validate and navigate.
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr && event.what == evKeyDown &&
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        event.keyDown.keyCode == kbEnter && current == bar_) {
        std::array<char, kUrlMaxLen + 1> buf{};
        bar_->getData(buf.data());
        const std::string url(buf.data());
        clearEvent(event);
        if (is_navigable(url)) {
            view_->navigate_to(url);
        }
        return;
    }
    TWindow::handleEvent(event);
}

void BrowserWindow::navigate_back() {
    view_->navigate_back();
}
void BrowserWindow::navigate_forward() {
    view_->navigate_forward();
}
void BrowserWindow::reload() {
    view_->reload();
}

}  // namespace tvshow::app
