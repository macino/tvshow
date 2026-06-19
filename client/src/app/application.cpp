#include "tvshow/app/application.hpp"

#define Uses_TDialog
#define Uses_TListViewer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_MsgBox
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#include "tvshow/app/browser_window.hpp"
#include "tvshow/app/commands.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::app {

namespace {

constexpr int kUrlMaxLen = 255;
constexpr int kUrlBufSize = kUrlMaxLen + 1;
constexpr size_t k_max_history = 500;

std::string history_file_path() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        return {};
    }
    return std::string(home) + "/.local/share/tvshow/history";
}

void load_history(SharedBrowsingState& state) {
    const std::string path = history_file_path();
    if (path.empty()) {
        return;
    }
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            state.history.push_back(line);
            state.visited.insert(line);
        }
    }
}

void save_history(const SharedBrowsingState& state) {
    const std::string path = history_file_path();
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (ec) {
        return;
    }
    std::ofstream file(path);
    const auto& hist = state.history;
    const size_t start = hist.size() > k_max_history ? hist.size() - k_max_history : 0;
    for (size_t i = start; i < hist.size(); ++i) {
        file << hist[i] << '\n';
    }
}

bool is_navigable(const std::string& url) {
    return util::Url::parse(url).has_value() || url.starts_with("file://");
}

// Lightweight TListViewer for the window-list dialog.
// Holds a pointer to an externally owned vector of strings (lives for the
// duration of the modal dialog execution).
class WindowListViewer : public TListViewer {
public:
    WindowListViewer(const TRect& bounds, const std::vector<std::string>* items)
        : TListViewer(bounds, 1, nullptr, nullptr), items_(items) {
        setRange(static_cast<short>(items_->size()));
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void getText(char* dest, short item, short maxLen) override {
        const auto idx = static_cast<size_t>(item);
        if (idx < items_->size()) {
            std::strncpy(dest, (*items_)[idx].c_str(), static_cast<size_t>(maxLen));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dest[maxLen] = '\0';
    }

    void handleEvent(TEvent& event) override {
        const bool what_key =
            event.what == evKeyDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool what_down =
            event.what == evMouseDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool dbl_click = what_down && ((event.mouse.eventFlags & meDoubleClick) !=
                                             0);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool enter_key =
            what_key &&
            (event.keyDown.keyCode == kbEnter);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool esc_key =
            what_key &&
            (event.keyDown.keyCode == kbEsc);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        if (dbl_click || enter_key) {
            endModal(cmOK);
            clearEvent(event);
        } else if (esc_key) {
            endModal(cmCancel);
            clearEvent(event);
        } else {
            TListViewer::handleEvent(event);
        }
    }

private:
    const std::vector<std::string>* items_;
};

struct CollectWindowsCtx {
    std::vector<std::string>* titles;
    std::vector<BrowserWindow*>* wins;
};

void collect_windows_cb(TView* v, void* arg) {
    auto* ctx = static_cast<CollectWindowsCtx*>(arg);
    auto* bw = dynamic_cast<BrowserWindow*>(v);
    if (bw != nullptr) {
        ctx->titles->emplace_back(bw->current_url());
        ctx->wins->push_back(bw);
    }
}

}  // namespace

Application::Application(AddressBarMode mode)
    : TProgInit(&Application::initStatusLine, &Application::initMenuBar, &Application::initDeskTop),
      mode_(mode) {
    load_history(shared_browsing_state_);
}

void Application::shutDown() {
    save_history(shared_browsing_state_);
    TApplication::shutDown();
}

auto Application::initStatusLine(TRect r) -> TStatusLine* {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
                                  *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                                  *new TStatusItem("~Alt-\x11~ Back", kbAltLeft, cmBack) +
                                  *new TStatusItem("~Ctrl-L~ URL", kbCtrlL, cmOpenUrl) +
                                  *new TStatusItem(nullptr, kbF10, cmMenu));
}

auto Application::initMenuBar(TRect r) -> TMenuBar* {
    r.b.y = r.a.y + 1;
    return new TMenuBar(
        r, *new TSubMenu("~F~ile", kbAltF) +
               *new TMenuItem("~N~ew Tab", cmNewTab, kbCtrlT, hcNoContext, "Ctrl-T") +
               *new TMenuItem("~O~pen URL...", cmOpenUrl, kbCtrlL, hcNoContext, "Ctrl-L") +
               *new TMenuItem("~R~eload", cmReload, kbF5, hcNoContext, "F5") +
               *new TMenuItem("~C~lose Tab", cmCloseTab, kbCtrlW, hcNoContext, "Ctrl-W") +
               newLine() + *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X") +
               *new TSubMenu("~N~avigate", kbAltN) +
               *new TMenuItem("~B~ack", cmBack, kbNoKey, hcNoContext, "Alt-\x11") +
               *new TMenuItem("~F~orward", cmForward, kbNoKey, hcNoContext, "Alt-\x10") +
               *new TSubMenu("~V~iew", kbAltV) +
               *new TMenuItem("~C~ascade", cmCascade, kbNoKey, hcNoContext) +
               *new TMenuItem("~T~ile", cmTile, kbNoKey, hcNoContext) +
               *new TSubMenu("~W~indow", kbAltW) +
               *new TMenuItem("~W~indow List...", cmWindowList, kbNoKey, hcNoContext));
}

BrowserWindow* Application::active_browser_window() {
    if (deskTop == nullptr || deskTop->current == nullptr) {
        return nullptr;
    }
    return dynamic_cast<BrowserWindow*>(deskTop->current);
}

void Application::show_window_list() {  // NOLINT(readability-convert-member-functions-to-static)
    if (deskTop == nullptr) {
        return;
    }

    std::vector<std::string> titles;
    std::vector<BrowserWindow*> windows;
    CollectWindowsCtx ctx{&titles, &windows};
    deskTop->forEach(collect_windows_cb, &ctx);

    if (windows.empty()) {
        return;
    }

    constexpr int kDlgW = 52;
    const int count = static_cast<int>(windows.size());
    const int h = std::min(count + 2, 18);
    const TRect dlg_r((deskTop->size.x - kDlgW) / 2, (deskTop->size.y - h) / 2,
                      (deskTop->size.x + kDlgW) / 2, (deskTop->size.y + h) / 2);
    auto* dlg = new TDialog(dlg_r, "Windows");

    const TRect list_r(1, 1, kDlgW - 2, h - 1);
    auto* viewer = new WindowListViewer(list_r, &titles);
    dlg->insert(viewer);

    const unsigned short res = deskTop->execView(dlg);
    const short sel = viewer->focused;
    TObject::destroy(dlg);

    if (res == cmOK && sel >= 0 && sel < count) {
        windows[static_cast<size_t>(sel)]->select();
    }
}

void Application::handleEvent(TEvent& event) {
    TApplication::handleEvent(event);
    if (event.what != evCommand) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    switch (event.message.command) {
    case cmNewTab: {
        clearEvent(event);
        std::array<char, kUrlBufSize> buf{};
        if (inputBox("New Tab", "Enter URL:", buf.data(), kUrlMaxLen) != cmOK) {
            return;
        }
        const std::string url(buf.data());
        if (is_navigable(url)) {
            open_url(url);
        }
        return;
    }
    case cmOpenUrl: {
        clearEvent(event);
        if (mode_ == AddressBarMode::Persistent) {
            // Delegate to the window's persistent bar (Tab-completion lives there).
            if (BrowserWindow* win = active_browser_window()) {
                win->focus_address_bar();
            }
            return;
        }
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
        if (BrowserWindow* win = active_browser_window()) {
            win->navigate(url);
        } else {
            open_url(url);
        }
        return;
    }
    case cmReload:
        clearEvent(event);
        if (BrowserWindow* win = active_browser_window()) {
            win->reload();
        }
        return;
    case cmCloseTab:
        clearEvent(event);
        if (BrowserWindow* win = active_browser_window()) {
            win->close();
        }
        return;
    case cmBack:
        clearEvent(event);
        if (BrowserWindow* win = active_browser_window()) {
            win->navigate_back();
        }
        return;
    case cmForward:
        clearEvent(event);
        if (BrowserWindow* win = active_browser_window()) {
            win->navigate_forward();
        }
        return;
    case cmWindowList:
        clearEvent(event);
        show_window_list();
        return;
    default:
        return;
    }
}

void Application::open_url(std::string_view url) {
    const layout::Viewport vp{std::max(1, deskTop->size.x), std::max(1, deskTop->size.y)};
    auto page = load_page(url, vp);
    if (!page) {
        return;
    }

    const TRect bounds = deskTop->getExtent();
    auto* win = new BrowserWindow(bounds, mode_, std::move(*page), &shared_browsing_state_);
    deskTop->insert(win);
}

}  // namespace tvshow::app
