#pragma once

#define Uses_TView
#include "tvshow/app/bookmarks.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/layout/form_focus.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/net/cookie_jar.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"

#include <tvision/tv.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

struct TScrollBar;

namespace tvshow::app {

// History and visited-URL state shared across all browser tabs in one process.
// Owned by Application, passed by pointer to each BrowserView.
struct SharedBrowsingState {
    std::vector<std::string> history;         // all visited URLs in order (for autocomplete)
    std::unordered_set<std::string> visited;  // set form of history (for link coloring)
    net::CookieJar cookie_jar;               // session-scoped cookie store
    BookmarkStore bookmarks;                 // loaded once at startup, persisted on change
};

// Hosts one loaded Page: renders it, tracks the focused link or form control
// (SPEC §12.1 / §13.2 Tab/Shift-Tab cycling), and resolves/navigates
// Enter-on-link, maintaining a simple per-window history stack.
class BrowserView : public TView {
public:
    // shared may be null (e.g., in standalone/test contexts).
    BrowserView(const TRect& bounds, Page page, SharedBrowsingState* shared = nullptr);

    void draw() override;
    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    ~BrowserView() override;

    // Navigate to url, pushing onto history. No-op on load failure.
    void navigate_to(std::string_view url);
    void navigate_back();
    void navigate_forward();
    void reload();

    // Find-in-page: show a dialog, scan the rendered grid, scroll to first hit.
    // Subsequent 'n'/'N' keys cycle through hits; Esc clears.
    void show_find_dialog();

    // Bookmarks: show CRUD picker dialog (Ctrl-B).
    void show_bookmarks_dialog();

    [[nodiscard]] const Page& page() const { return page_; }
    // History for URL-bar autocomplete: shared history when available, else per-window.
    [[nodiscard]] const std::vector<std::string>& history() const {
        return (shared_ != nullptr) ? shared_->history : history_;
    }

    // Wire a vertical scrollbar managed by BrowserWindow. May be null.
    void set_vscroll(TScrollBar* sb) { vscroll_ = sb; }

    // Scroll to absolute row (clamped to [0, scroll_limit()]).
    void scroll_to(int row);

    // Maximum scroll offset in rows.
    [[nodiscard]] int scroll_limit() const;

    // Called from Application::idle() on the main thread to animate the spinner
    // and apply a completed page load.
    void tick_if_loading();

private:
    Page page_;
    std::vector<layout::Link> links_;
    std::vector<layout::FormFocus> form_controls_;
    render::FormValues form_values_;
    int focused_ = -1;    // index into links_ + form_controls_ (0..n-1), or -1
    int scroll_row_ = 0;  // current vertical scroll offset in rows

    TScrollBar* vscroll_{nullptr};  // non-owning; managed by BrowserWindow

    std::vector<std::string> history_;               // per-window, for back/forward only
    std::unordered_set<std::string> local_visited_;  // fallback when shared_ is null
    BookmarkStore local_bookmarks_;                  // fallback when shared_ is null
    size_t history_pos_ = 0;

    SharedBrowsingState* shared_{nullptr};  // non-owning; null in standalone mode

    // Async loading state.
    struct PendingLoad {
        std::optional<Page> page;
        std::string url;
        std::string fragment;
        bool push_history{false};
    };
    std::atomic<bool> loading_{false};
    std::atomic<bool> page_ready_{false};  // set by thread, cleared by apply_loaded_page
    std::atomic<bool> load_cancelled_{false};
    std::chrono::steady_clock::time_point load_start_;
    std::thread loader_thread_;
    std::mutex pending_mutex_;
    std::optional<PendingLoad> pending_load_;

    [[nodiscard]] const std::unordered_set<std::string>& visited_set() const noexcept {
        return (shared_ != nullptr) ? shared_->visited : local_visited_;
    }
    void record_visit(const std::string& url);
    // Apply the result from pending_load_ (main thread only).
    void apply_loaded_page();

    [[nodiscard]] int total_focusables() const;
    [[nodiscard]] bool is_link_focused() const;
    [[nodiscard]] const layout::FormFocus* focused_fc() const;

    void relayout();
    void sync_vscroll();
    [[nodiscard]] render::CharGrid render_grid() const;
    void navigate(const std::string& url, bool push_history);
    // Hit-test pt (content coords) against links/form controls, handle and clear
    // event on hit. Returns true if the event was consumed.
    bool handle_mouse_hit(Point pt, TEvent& event);
    void focus_next(int direction);
    void handle_form_input(unsigned keyCode);
    // Move the focused <select>'s selected option by direction (+1 = down, -1 = up).
    void cycle_select_option(int direction);
    // Show a modal option-picker dialog for the given select control.
    void show_select_popup(const layout::FormFocus& fc);
    void submit_form();

    // Find-in-page state.
    std::string search_term_;
    std::vector<Point> search_hits_;
    int search_hit_idx_ = -1;
    int search_len_ = 0;  // search term length in codepoints

    void find_matches_in_page(std::string_view term);
    void navigate_to_hit(int dir);  // +1 = next, -1 = prev

    bool debug_overlay_ = false;  // Ctrl-D toggles box-outline overlay
};

}  // namespace tvshow::app
