#include "tvshow/app/application.hpp"

#include "tvshow/app/browser_window.hpp"

#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_MsgBox
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#include "tvshow/app/page.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace tvshow::app {

namespace {
// inputBox limit is uchar (max 255 bytes); keep buf one byte larger.
constexpr int kUrlMaxLen = 255;
constexpr int kUrlBufSize = kUrlMaxLen + 1;

bool is_navigable(const std::string& url) {
    return util::Url::parse(url).has_value() || url.starts_with("file://");
}
}  // namespace

Application::Application(AddressBarMode mode)
    : TProgInit(&Application::initStatusLine, &Application::initMenuBar, &Application::initDeskTop),
      mode_(mode) {}

auto Application::initStatusLine(TRect r) -> TStatusLine* {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
                                  *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                                  *new TStatusItem("~Ctrl-L~ URL", kbCtrlL, 0) +
                                  *new TStatusItem(nullptr, kbF10, cmMenu));
}

auto Application::initMenuBar(TRect r) -> TMenuBar* {
    r.b.y = r.a.y + 1;
    return new TMenuBar(r, *new TSubMenu("~F~ile", kbAltF) +
                               *new TMenuItem("E~x~it", cmQuit, cmQuit, hcNoContext, "Alt-X"));
}

BrowserWindow* Application::active_browser_window() {
    if (deskTop == nullptr || deskTop->current == nullptr) {
        return nullptr;
    }
    return dynamic_cast<BrowserWindow*>(deskTop->current);
}

void Application::handleEvent(TEvent& event) {
    TApplication::handleEvent(event);

    if (event.what != evKeyDown) {
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    if (event.keyDown.keyCode != kbCtrlL) {
        return;
    }

    clearEvent(event);

    if (mode_ == AddressBarMode::Persistent) {
        // Focus the address bar of the active window; open new window if none.
        BrowserWindow* win = active_browser_window();
        if (win != nullptr) {
            win->focus_address_bar();
            return;
        }
    }

    // Modal: prompt for URL via inputBox.
    std::array<char, kUrlBufSize> buf{};
    if (const BrowserWindow* win = active_browser_window()) {
        std::strncpy(buf.data(), std::string(win->current_url()).c_str(), kUrlMaxLen);
    }
    if (inputBox("Open URL", "Enter URL:", buf.data(), kUrlMaxLen) != cmOK) {
        return;
    }
    const std::string url(buf.data());
    if (!is_navigable(url)) {
        return;
    }
    // Navigate in the active window, or open a new one if none.
    if (BrowserWindow* win = active_browser_window()) {
        win->navigate(url);
    } else {
        open_url(url);
    }
}

void Application::open_url(std::string_view url) {
    const layout::Viewport vp{std::max(1, deskTop->size.x), std::max(1, deskTop->size.y)};
    auto page = load_page(url, vp);
    if (!page) {
        return;
    }

    const TRect bounds = deskTop->getExtent();
    auto* win = new BrowserWindow(bounds, mode_, std::move(*page));
    deskTop->insert(win);
}

}  // namespace tvshow::app
