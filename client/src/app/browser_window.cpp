#include "tvshow/app/browser_window.hpp"

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TEvent
#define Uses_TKeys
#define Uses_TScrollBar
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
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

void BrowserWindow::handle_url_completion(bool reverse) {
    std::array<char, kUrlMaxLen + 1> buf{};
    bar_->getData(buf.data());
    const std::string cur(buf.data());

    // Check whether we can continue the current session or must start a new one.
    const bool same_session = completion_valid_ && cur == completion_current_;
    if (!same_session) {
        completion_prefix_ = cur;
        completion_candidates_.clear();
        completion_idx_ = 0;
        completion_valid_ = true;
        // Collect unique history entries that extend the current prefix (most recent first).
        std::unordered_set<std::string> seen;
        for (const auto& url : view_->history() | std::views::reverse) {
            if (url.size() > cur.size() && url.starts_with(cur) && seen.insert(url).second) {
                completion_candidates_.push_back(url);
            }
        }
    } else {
        // Advance (or reverse) through existing candidates.
        if (!completion_candidates_.empty()) {
            const std::size_t n = completion_candidates_.size();
            completion_idx_ = reverse ? (completion_idx_ + n - 1) % n : (completion_idx_ + 1) % n;
        }
    }

    if (completion_candidates_.empty()) {
        return;
    }
    const std::string& suggestion = completion_candidates_[completion_idx_];
    completion_current_ = suggestion;
    std::array<char, kUrlMaxLen + 1> sbuf{};
    std::strncpy(sbuf.data(), suggestion.c_str(), kUrlMaxLen);
    bar_->setData(sbuf.data());
    bar_->selectAll(true);
    bar_->drawView();
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
    // Persistent bar key handling (Tab completion + Enter navigation).
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr && event.what == evKeyDown &&
        current == bar_) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const uint16_t key = event.keyDown.keyCode;
        if (key == kbTab) {
            handle_url_completion(false);
            clearEvent(event);
            return;
        }
        if (key == kbShiftTab) {
            handle_url_completion(true);
            clearEvent(event);
            return;
        }
        if (key == kbEnter) {
            std::array<char, kUrlMaxLen + 1> buf{};
            bar_->getData(buf.data());
            const std::string url(buf.data());
            completion_valid_ = false;
            clearEvent(event);
            if (is_navigable(url)) {
                view_->navigate_to(url);
            }
            return;
        }
        // Any other key resets the completion session.
        completion_valid_ = false;
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
