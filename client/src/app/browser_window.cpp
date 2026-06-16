#include "tvshow/app/browser_window.hpp"

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TEvent
#define Uses_TKeys
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

    TRect view_rect = inner;
    if (mode_ == AddressBarMode::Persistent) {
        view_rect.a.y += kBarHeight;
    }
    view_ = new BrowserView(view_rect, std::move(page));
    insert(view_);
}

void BrowserWindow::reposition(const TRect& inner) {
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr) {
        TRect bar_rect = inner;
        bar_rect.b.y = bar_rect.a.y + kBarHeight;
        bar_->changeBounds(bar_rect);
    }
    TRect view_rect = inner;
    if (mode_ == AddressBarMode::Persistent) {
        view_rect.a.y += kBarHeight;
    }
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

}  // namespace tvshow::app
