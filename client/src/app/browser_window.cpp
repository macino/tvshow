#include "tvshow/app/browser_window.hpp"

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/dom/node.hpp"

#define Uses_TEvent
#define Uses_TFrame
#define Uses_TKeys
#define Uses_TScrollBar
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <array>
#include <charconv>
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

std::string normalize_url(const std::string& url) {
    if (url.empty()) return url;
    if (url.starts_with("http://") || url.starts_with("https://") ||
        url.starts_with("file://")) {
        return url;
    }
    return "https://" + url;
}

constexpr int kMinHintWidth = 20;
constexpr int kMinHintHeight = 8;

// Parses "WxH" (e.g. "26x14"); returns {0, 0} on any malformed input, which
// callers treat as "no hint" -- same degrade-gracefully stance as the rest
// of this codebase rather than rejecting the whole page load over it.
TPoint parse_size_hint(std::string_view s) {
    const auto x_pos = s.find('x');
    if (x_pos == std::string_view::npos) {
        return {0, 0};
    }
    int w = 0;
    int h = 0;
    const auto w_res = std::from_chars(s.data(), s.data() + x_pos, w);
    const auto h_res = std::from_chars(s.data() + x_pos + 1, s.data() + s.size(), h);
    if (w_res.ec != std::errc{} || h_res.ec != std::errc{} || w <= 0 || h <= 0) {
        return {0, 0};
    }
    return {w, h};
}
}  // namespace

BrowserWindow::BrowserWindow(const TRect& bounds, AddressBarMode mode, Page page,
                             SharedBrowsingState* shared, ForcedStyle initial_style)
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
    view_ = new BrowserView(view_rect, std::move(page), shared, initial_style);
    view_->set_vscroll(vscroll_);
    insert(view_);
    apply_page_window_hints();
}

void BrowserWindow::apply_page_window_hints() {
    last_page_generation_ = view_->page_generation();
    const dom::Document& doc = view_->page().doc;

    const TPoint hint_size = parse_size_hint(dom::find_meta_content(doc, "tvshow-window-size"));
    if (hint_size.x > 0 && hint_size.y > 0) {
        const TPoint desk = TProgram::deskTop != nullptr ? TProgram::deskTop->size : hint_size;
        const int w = std::clamp(hint_size.x, kMinHintWidth, std::max(kMinHintWidth, desk.x));
        const int h = std::clamp(hint_size.y, kMinHintHeight, std::max(kMinHintHeight, desk.y));
        TRect r = getBounds();
        r.b.x = r.a.x + w;
        r.b.y = r.a.y + h;
        changeBounds(r);
        // changeBounds() alone leaves stale pixels where the window used to
        // extend -- shrinking a window doesn't repaint what's now behind it.
        // Interactive resize (dragging the handle) gets this for free from
        // tvision's own drag loop; a one-shot programmatic resize needs to
        // ask the owner to repaint explicitly. TGroup::redraw() (a full
        // drawSubViews() pass, not TView::drawView()) is the safe way to do
        // that -- TGroup::drawUnderRect() looked like the more targeted
        // tool but segfaulted here (likely a contract this call site
        // doesn't satisfy, called mid- Application::idle()'s window
        // iteration); redraw() is a small correctness/perf tradeoff
        // (repaints everything, not just the exposed delta) for something
        // that reliably doesn't crash.
        if (owner != nullptr) {
            owner->redraw();
        }
    }

    const std::string_view color = dom::find_meta_content(doc, "tvshow-window-color");
    if (color == "gray") {
        palette_hint_ = PaletteHint::Gray;
    } else if (color == "cyan") {
        palette_hint_ = PaletteHint::Cyan;
    } else if (color == "blue") {
        palette_hint_ = PaletteHint::Blue;
    } else {
        palette_hint_ = PaletteHint::None;
    }
    drawView();
}

TPalette& BrowserWindow::getPalette() const {
    // cpGrayDialog/cpCyanDialog/cpBlueDialog -- same 32-entry tables (and
    // same reasoning) adr-native-demo-windows's native windows use; see
    // CalculatorWindow::getPalette() for why the shorter default TWindow
    // palette isn't enough once real TButton/TInputLine widgets are inside.
    switch (palette_hint_) {
    case PaletteHint::Gray: {
        static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
        return pal;
    }
    case PaletteHint::Cyan: {
        static TPalette pal(cpCyanDialog, sizeof(cpCyanDialog) - 1);
        return pal;
    }
    case PaletteHint::Blue: {
        static TPalette pal(cpBlueDialog, sizeof(cpBlueDialog) - 1);
        return pal;
    }
    case PaletteHint::None:
    default:
        return TWindow::getPalette();
    }
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
        // Clear bar so Tab immediately searches all history.
        std::array<char, kUrlMaxLen + 1> buf{};
        buf[0] = '\0';
        bar_->setData(buf.data());
        completion_valid_ = false;
        bar_->select();
        bar_->drawView();
    }
}

void BrowserWindow::navigate(std::string_view url) {
    const std::string s = normalize_url(std::string(url));
    if (!is_navigable(s)) {
        return;
    }
    view_->navigate_to(s);
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr) {
        std::array<char, kUrlMaxLen + 1> nbuf{};
        std::strncpy(nbuf.data(), view_->page().url.c_str(), kUrlMaxLen);
        bar_->setData(nbuf.data());
        bar_->drawView();
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
    // Persistent bar key handling (Down/Up completion + Enter navigation).
    if (mode_ == AddressBarMode::Persistent && bar_ != nullptr && event.what == evKeyDown &&
        current == bar_) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const uint16_t key = event.keyDown.keyCode;
        if (key == kbDown) {
            handle_url_completion(false);
            clearEvent(event);
            return;
        }
        if (key == kbUp) {
            handle_url_completion(true);
            clearEvent(event);
            return;
        }
        // Tab from bar moves focus to page content so the user can reach form
        // fields and links without a mouse.  Shift+Tab cycles back to the bar.
        if (key == kbTab) {
            view_->select();
            view_->focus_first();
            completion_valid_ = false;
            clearEvent(event);
            return;
        }
        if (key == kbEnter) {
            std::array<char, kUrlMaxLen + 1> buf{};
            bar_->getData(buf.data());
            const std::string url = normalize_url(std::string(buf.data()));
            completion_valid_ = false;
            clearEvent(event);
            if (is_navigable(url)) {
                view_->navigate_to(url);
                // Reflect the actual loaded URL back in the bar.
                std::array<char, kUrlMaxLen + 1> nbuf{};
                std::strncpy(nbuf.data(), view_->page().url.c_str(), kUrlMaxLen);
                bar_->setData(nbuf.data());
                bar_->drawView();
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

void BrowserWindow::tick_if_loading() {
    view_->tick_if_loading();
    if (view_->page_generation() != last_page_generation_) {
        apply_page_window_hints();
    }
    // Sync title and address bar to the URL of the page that just landed.
    const std::string_view cur = current_url();
    if (title == nullptr || std::string_view(title) != cur) {
        delete[] title;  // NOLINT
        title = newStr(cur);
        frame->drawView();
        if (mode_ == AddressBarMode::Persistent && bar_ != nullptr) {
            std::array<char, kUrlMaxLen + 1> buf{};
            std::strncpy(buf.data(), std::string(cur).c_str(), kUrlMaxLen);
            bar_->setData(buf.data());
            bar_->drawView();
        }
    }
}

}  // namespace tvshow::app
