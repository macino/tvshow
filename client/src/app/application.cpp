#include "tvshow/app/application.hpp"

#define Uses_TDialog
#define Uses_TInputLine
#define Uses_TListViewer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_MsgBox
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#include "tvshow/app/bookmarks.hpp"
#include "tvshow/app/browser_window.hpp"
#include "tvshow/app/commands.hpp"
#include "tvshow/app/char_chart_window.hpp"
#include "tvshow/app/calc_window.hpp"
#include "tvshow/app/calendar_window.hpp"
#include "tvshow/app/event_viewer_window.hpp"
#include "tvshow/app/puzzle_window.hpp"
#include "tvshow/app/translator_window.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/app/settings_dialog.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/blocklist.hpp"
#include "tvshow/net/cookie_jar.hpp"
#include "tvshow/net/extension_server.hpp"
#include "tvshow/util/config.hpp"
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
#include <unordered_set>
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

// If `url` has no http/https scheme, prepend "https://".
// E.g. "redmine.previo.info/login" -> "https://redmine.previo.info/login".
std::string normalize_url(const std::string& url) {
    if (url.empty()) return url;
    if (url.starts_with("http://") || url.starts_with("https://") ||
        url.starts_with("file://")) {
        return url;
    }
    return "https://" + url;
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

// ── URL history picker ───────────────────────────────────────────────────────

// TListViewer that fills a TInputLine whenever the focused item changes.
// Enter or double-click closes the parent dialog with cmOK.
class HistoryListViewer : public TListViewer {
public:
    HistoryListViewer(const TRect& bounds, const std::vector<std::string>* items,
                      TInputLine* bar, int bar_max_len)
        : TListViewer(bounds, 1, nullptr, nullptr),
          items_(items),
          bar_(bar),
          bar_max_len_(bar_max_len) {
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

    void focusItem(short item) override {
        TListViewer::focusItem(item);
        const auto idx = static_cast<size_t>(item);
        if (bar_ == nullptr || idx >= items_->size()) {
            return;
        }
        std::vector<char> buf(static_cast<size_t>(bar_max_len_) + 1, '\0');
        std::strncpy(buf.data(), (*items_)[idx].c_str(), static_cast<size_t>(bar_max_len_));
        bar_->setData(buf.data());
        bar_->drawView();
    }

    void handleEvent(TEvent& event) override {
        const bool is_key =
            event.what == evKeyDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool is_down =
            event.what == evMouseDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool dbl_click = is_down && ((event.mouse.eventFlags &  // NOLINT(cppcoreguidelines-pro-type-union-access)
                                            meDoubleClick) != 0);
        const bool enter_key =
            is_key &&
            (event.keyDown.keyCode == kbEnter);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool esc_key =
            is_key &&
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
    TInputLine* bar_{nullptr};
    int bar_max_len_;
};

// TDialog subclass that handles keyboard inside the URL picker correctly:
//   - Enter (from anywhere) → close cmOK without dispatching to TInputLine
//     (TInputLine would consume kbEnter or insert a space; TDialog alone never
//     closes because it broadcasts cmDefault with no button to handle it).
//   - Down arrow while input is focused → shift focus to the history list.
class UrlPickerDialog : public TDialog {
public:
    UrlPickerDialog(const TRect& r, const char* dlg_title)
        : TWindowInit(&TWindow::initFrame), TDialog(r, dlg_title) {}

    TInputLine* bar_{nullptr};
    HistoryListViewer* lv_{nullptr};

    void handleEvent(TEvent& event) override {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        if (event.what == evKeyDown) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            const uint16_t kc = event.keyDown.keyCode;
            if (kc == kbEnter) {
                endModal(cmOK);
                clearEvent(event);
                return;
            }
            if (kc == kbDown && current == bar_ && lv_ != nullptr) {
                lv_->select();
                clearEvent(event);
                return;
            }
        }
        TDialog::handleEvent(event);
    }
};

// Builds a deduplicated (most-recent-first) view of the history vector.
std::vector<std::string> dedupe_history(const std::vector<std::string>& history) {
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (seen.insert(*it).second) {
            out.push_back(*it);
        }
    }
    return out;
}

// Shows a URL picker dialog: TInputLine at top (pre-filled with initial_url),
// deduplicated history list below.  Returns the chosen URL or "" on cancel.
std::string show_url_picker(TGroup* desktop, const char* title,
                            std::string_view initial_url,
                            const std::vector<std::string>& history) {
    if (desktop == nullptr) {
        return {};
    }
    constexpr int kPickerUrlMax = 511;
    const std::vector<std::string> items = dedupe_history(history);
    const int n = static_cast<int>(items.size());
    const int list_h = std::min(n, 12);
    const int dlg_h = 2 + 1 + (list_h > 0 ? list_h : 0);  // frame + input + list
    constexpr int kDlgW = 62;
    const TRect desk = desktop->getBounds();
    const TRect dlg_r{(desk.b.x - kDlgW) / 2, (desk.b.y - dlg_h) / 2,
                      (desk.b.x + kDlgW) / 2, (desk.b.y + dlg_h) / 2};
    auto* dlg = new UrlPickerDialog(dlg_r, title);

    const TRect bar_r{1, 1, kDlgW - 2, 2};
    auto* bar = new TInputLine(bar_r, kPickerUrlMax);
    {
        std::array<char, kPickerUrlMax + 1> ibuf{};
        std::strncpy(ibuf.data(), std::string(initial_url).c_str(), kPickerUrlMax);
        bar->setData(ibuf.data());
    }
    dlg->insert(bar);
    dlg->bar_ = bar;

    if (list_h > 0) {
        const TRect list_r{1, 2, kDlgW - 2, 2 + list_h};
        auto* lv = new HistoryListViewer(list_r, &items, bar, kPickerUrlMax);
        dlg->insert(lv);
        dlg->lv_ = lv;
    }
    bar->select();

    const unsigned short res = desktop->execView(dlg);
    std::array<char, kPickerUrlMax + 1> rbuf{};
    bar->getData(rbuf.data());
    TObject::destroy(dlg);

    if (res != cmOK) {
        return {};
    }
    return std::string(rbuf.data());
}

// ── idle helper ─────────────────────────────────────────────────────────────

static void tick_loading_window(TView* v, void* /*arg*/) {
    if (auto* bw = dynamic_cast<BrowserWindow*>(v)) {
        bw->tick_if_loading();
    }
}

// adr-translator-native-window: pulls the in-flight translate request's output, if any.
static void poll_translator_window(TView* v, void* /*arg*/) {
    if (auto* tw = dynamic_cast<TranslatorWindow*>(v)) {
        tw->poll();
    }
}

// ── window-list helpers ──────────────────────────────────────────────────────

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
    shared_browsing_state_.bookmarks = load_bookmarks();
    net::load_cookie_jar(shared_browsing_state_.cookie_jar);
    shared_browsing_state_.blocklist = net::load_blocklist();
}

Application::~Application() = default;

int Application::ensure_extension_server() {
    if (extension_server_ && extension_server_->running()) {
        return extension_server_->port();
    }
    const util::Config cfg = util::load_config();
    extension_server_ =
        std::make_unique<net::ExtensionServer>(std::vector<std::string>{TVSHOW_EXTENSIONS_DIR});
    if (!extension_server_->start(cfg.extension_server_port)) {
        extension_server_.reset();
        return 0;
    }
    return extension_server_->port();
}

void Application::shutDown() {
    save_history(shared_browsing_state_);
    net::save_cookie_jar(shared_browsing_state_.cookie_jar);
    TApplication::shutDown();
}

auto Application::initStatusLine(TRect r) -> TStatusLine* {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
                                  *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                                  *new TStatusItem("~Alt-\x11~ Back", kbAltLeft, cmBack) +
                                  *new TStatusItem("~Alt-\x10~ Fwd", kbAltRight, cmForward) +
                                  *new TStatusItem("~Ctrl-R~ Reload", kbCtrlR, cmReload) +
                                  *new TStatusItem("~Ctrl-L~ URL", kbCtrlL, cmOpenUrl) +
                                  *new TStatusItem(nullptr, kbF10, cmMenu));
}

auto Application::initMenuBar(TRect r) -> TMenuBar* {
    r.b.y = r.a.y + 1;
    return new TMenuBar(
        r, *new TSubMenu("~F~ile", kbAltF) +
               *new TMenuItem("~N~ew Tab", cmNewTab, kbCtrlT, hcNoContext, "Ctrl-T") +
               *new TMenuItem("~O~pen URL...", cmOpenUrl, kbCtrlL, hcNoContext, "Ctrl-L") +
               *new TMenuItem("~C~lose Tab", cmCloseTab, kbCtrlW, hcNoContext, "Ctrl-W") +
               newLine() + *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X") +
               *new TSubMenu("~N~avigate", kbAltN) +
               *new TMenuItem("~B~ack", cmBack, kbAltLeft, hcNoContext, "Alt-\x11") +
               *new TMenuItem("~F~orward", cmForward, kbAltRight, hcNoContext, "Alt-\x10") +
               *new TMenuItem("~R~eload", cmReload, kbCtrlR, hcNoContext, "Ctrl-R") +
               *new TSubMenu("~V~iew", kbAltV) +
               *new TMenuItem("~C~ascade", cmCascade, kbNoKey, hcNoContext) +
               *new TMenuItem("~T~ile", cmTile, kbNoKey, hcNoContext) +
               newLine() +
               *new TMenuItem("Style: ~A~uto",    cmStyleAuto,    kbNoKey, hcNoContext) +
               *new TMenuItem("Style: ~t~vision", cmStyleTvision, kbNoKey, hcNoContext) +
               *new TMenuItem("Style: ~L~ight",   cmStyleLight,   kbNoKey, hcNoContext) +
               *new TMenuItem("Style: ~D~ark",    cmStyleDark,    kbNoKey, hcNoContext) +
               newLine() +
               *new TMenuItem("~S~ettings...", cmSettings, kbNoKey, hcNoContext) +
               *new TSubMenu("~W~indow", kbAltW) +
               *new TMenuItem("~C~lose", cmClose, kbAltF3, hcNoContext, "Alt-F3") +
               *new TMenuItem("~Z~oom", cmZoom, kbAltF5, hcNoContext, "Alt-F5") +
               *new TMenuItem("~M~ove/Resize", cmResize, kbAltF7, hcNoContext, "Alt-F7") +
               newLine() +
               *new TMenuItem("~W~indow List...", cmWindowList, kbNoKey, hcNoContext) +
               *new TSubMenu("~T~ools", kbAltT) +
               *new TMenuItem("~T~ranslate...", cmOpenTranslator, kbNoKey, hcNoContext) +
               newLine() +
               *new TMenuItem("~C~alculator", cmOpenCalculator, kbNoKey, hcNoContext) +
               *new TMenuItem("Cale~n~dar", cmOpenCalendarWin, kbNoKey, hcNoContext) +
               *new TMenuItem("~P~uzzle", cmOpenPuzzle, kbNoKey, hcNoContext) +
               *new TMenuItem("C~h~ar Chart", cmOpenCharChart, kbNoKey, hcNoContext) +
               *new TMenuItem("~E~vent Viewer", cmOpenEventViewer, kbNoKey, hcNoContext) +
               newLine() +
               *new TMenuItem("E~x~tension: Calculator (HTTP)...", cmOpenHttpCalculator, kbNoKey,
                              hcNoContext));
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

void Application::idle() {
    TProgram::idle();
    if (deskTop != nullptr) {
        deskTop->forEach(tick_loading_window, nullptr);
        deskTop->forEach(poll_translator_window, nullptr);
    }
}

void Application::handleEvent(TEvent& event) {
    // Handle F5 as reload shortcut (in addition to Ctrl+R in menu).
    if (event.what == evKeyDown &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.keyDown.keyCode == kbF5) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        clearEvent(event);
        if (BrowserWindow* win = active_browser_window()) {
            win->reload();
        }
        return;
    }

    TApplication::handleEvent(event);
    if (event.what != evCommand) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    switch (event.message.command) {
    case cmNewTab: {
        clearEvent(event);
        const std::string url = normalize_url(
            show_url_picker(deskTop, "New Tab", "", shared_browsing_state_.history));
        if (is_navigable(url)) {
            deferred_open_url_ = url;
            TEvent ev{};
            ev.what = evCommand;
            ev.message.command = cmDeferredOpen;
            ev.message.infoPtr = nullptr;
            putEvent(ev);
        }
        return;
    }
    case cmOpenUrl: {
        clearEvent(event);
        std::string_view prefill;
        std::string prefill_buf;
        if (const BrowserWindow* win = active_browser_window()) {
            prefill_buf = std::string(win->current_url());
            prefill = prefill_buf;
        }
        const std::string url = normalize_url(
            show_url_picker(deskTop, "Open URL", prefill, shared_browsing_state_.history));
        if (!is_navigable(url)) {
            return;
        }
        if (BrowserWindow* win = active_browser_window()) {
            win->navigate(url);
        } else {
            // Post cmDeferredOpen so open_url runs in the next main-loop iteration,
            // after the modal dialog has fully unwound from the tvision event stack.
            deferred_open_url_ = url;
            TEvent ev{};
            ev.what = evCommand;
            ev.message.command = cmDeferredOpen;
            ev.message.infoPtr = nullptr;
            putEvent(ev);
        }
        return;
    }
    case cmDeferredOpen: {
        clearEvent(event);
        if (!deferred_open_url_.empty()) {
            std::string url = std::move(deferred_open_url_);
            deferred_open_url_.clear();
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
    case cmOpenTranslator: {
        clearEvent(event);
        if (deskTop == nullptr) { return; }
        const util::Config cfg = util::load_config();
        auto* win = new TranslatorWindow(fixed_window_bounds(38, 12), cfg.translator_script);
        deskTop->insert(win);
        return;
    }
    case cmOpenCalculator:
        clearEvent(event);
        if (deskTop != nullptr) {
            deskTop->insert(new CalculatorWindow(fixed_window_bounds(26, 14)));
        }
        return;
    case cmOpenCalendarWin:
        clearEvent(event);
        if (deskTop != nullptr) {
            deskTop->insert(new CalendarWindow(fixed_window_bounds(22, 13)));
        }
        return;
    case cmOpenPuzzle:
        clearEvent(event);
        if (deskTop != nullptr) { deskTop->insert(new PuzzleWindow(fixed_window_bounds(22, 12))); }
        return;
    case cmOpenCharChart:
        clearEvent(event);
        if (deskTop != nullptr) {
            deskTop->insert(new CharChartWindow(fixed_window_bounds(64, 16)));
        }
        return;
    case cmOpenEventViewer:
        clearEvent(event);
        if (deskTop != nullptr) {
            deskTop->insert(new EventViewerWindow(fixed_window_bounds(60, 20)));
        }
        return;
    case cmOpenHttpCalculator: {
        clearEvent(event);
        const int port = ensure_extension_server();
        if (port == 0) {
            messageBox("Extension server failed to start.", mfError | mfOKButton);
            return;
        }
        open_url("http://127.0.0.1:" + std::to_string(port) + "/extensions/calculator/");
        return;
    }
    case cmSettings:
        clearEvent(event);
        show_settings_dialog(shared_browsing_state_);
        set_forced_style(shared_browsing_state_.forced_style);
        return;
    case cmStyleAuto:
    case cmStyleTvision:
    case cmStyleLight:
    case cmStyleDark: {
        const unsigned short cmd = event.message.command;
        clearEvent(event);
        const ForcedStyle fs = (cmd == cmStyleTvision) ? ForcedStyle::Tvision
                             : (cmd == cmStyleLight)   ? ForcedStyle::Light
                             : (cmd == cmStyleDark)    ? ForcedStyle::Dark
                                                       : ForcedStyle::Auto;
        set_active_window_style(fs);
        return;
    }
    default:
        return;
    }
}

TRect Application::next_window_bounds() {
    constexpr int kCascadeCols = 2;
    constexpr int kCascadeRows = 1;
    constexpr int kMinWidth = 20;
    constexpr int kMinHeight = 6;

    const TRect desk = deskTop->getExtent();
    const int dw = desk.b.x - desk.a.x;
    const int dh = desk.b.y - desk.a.y;

    // Each step shifts the origin by (kCascadeCols, kCascadeRows) and shrinks
    // the window by the same amount so windows don't go off-screen.
    const int max_steps_h = std::max(0, (dw - kMinWidth) / kCascadeCols);
    const int max_steps_v = std::max(0, (dh - kMinHeight) / kCascadeRows);
    const int max_steps = std::min(max_steps_h, max_steps_v);
    if (max_steps > 0) {
        cascade_step_ = cascade_step_ % max_steps;
    } else {
        cascade_step_ = 0;
    }

    const int x0 = desk.a.x + cascade_step_ * kCascadeCols;
    const int y0 = desk.a.y + cascade_step_ * kCascadeRows;
    ++cascade_step_;
    return {x0, y0, desk.b.x, desk.b.y};
}

TRect Application::fixed_window_bounds(int w, int h) {
    constexpr int kCascadeCols = 2;
    constexpr int kCascadeRows = 1;

    const TRect desk = deskTop->getExtent();
    const int dw = desk.b.x - desk.a.x;
    const int dh = desk.b.y - desk.a.y;
    w = std::min(w, dw);
    h = std::min(h, dh);

    const int base_x = desk.a.x + std::max(0, (dw - w) / 2);
    const int base_y = desk.a.y + std::max(0, (dh - h) / 2);

    const int max_steps_h = std::max(0, (dw - w) / kCascadeCols);
    const int max_steps_v = std::max(0, (dh - h) / kCascadeRows);
    const int max_steps = std::min(max_steps_h, max_steps_v);
    const int step = (max_steps > 0) ? (cascade_step_ % max_steps) : 0;
    ++cascade_step_;

    const int x0 = std::min(base_x + step * kCascadeCols, desk.b.x - w);
    const int y0 = std::min(base_y + step * kCascadeRows, desk.b.y - h);
    return {x0, y0, x0 + w, y0 + h};
}

void Application::open_url(std::string_view url) {
    const layout::Viewport vp{std::max(1, deskTop->size.x), std::max(1, deskTop->size.y)};
    // Open the window immediately with a blank page so the spinner is visible,
    // then kick off the async load — same path as navigating from an existing window.
    auto blank = load_page_from_error("", "", vp);
    if (!blank) return;
    blank->url = std::string(url);

    const TRect bounds = next_window_bounds();
    auto* win = new BrowserWindow(bounds, mode_, std::move(*blank), &shared_browsing_state_,
                                  shared_browsing_state_.forced_style);
    deskTop->insert(win);
    win->navigate(url);
}

// Sets the app-wide default forced style: seeds every *new* window opened
// from now on (open_url() reads shared_browsing_state_.forced_style), and
// — since this is the deliberate "change my persistent preference" path
// (startup --style/config default-style, and the Settings dialog after
// writing it to config.toml) — also re-applies it live to every window
// already open, matching what changing a saved preference implies.
void Application::set_forced_style(ForcedStyle fs) {
    shared_browsing_state_.forced_style = fs;
    if (deskTop == nullptr) {
        return;
    }
    deskTop->forEach([](TView* v, void*) {
        if (auto* bw = dynamic_cast<BrowserWindow*>(v)) {
            bw->apply_forced_style();
        }
    }, nullptr);
}

// View > Style: a quick, ephemeral override for *this* window only — does
// not touch shared_browsing_state_.forced_style, so it doesn't change what
// future new tabs open with, and doesn't affect any other already-open tab.
void Application::set_active_window_style(ForcedStyle fs) {
    if (BrowserWindow* win = active_browser_window()) {
        win->set_forced_style(fs);
    }
}

}  // namespace tvshow::app
