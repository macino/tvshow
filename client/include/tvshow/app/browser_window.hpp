#pragma once

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TInputLine
#define Uses_TScrollBar
#define Uses_TWindow
#include <tvision/tv.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::app {

enum class AddressBarMode : std::uint8_t { Modal, Persistent };

// BrowserWindow hosts BrowserView inside a TWindow.
//
// Modal mode (default): plain window with no permanent address bar.
//   Ctrl-L is handled by Application and opens an inputBox dialog.
//
// Persistent mode (--address-bar=persistent): a one-row TInputLine sits at
//   the top of the window; Enter in it validates the URL and navigates.
//   Content viewport is one row shorter.
class BrowserWindow : public TWindow {
public:
    BrowserWindow(const TRect& bounds, AddressBarMode mode, Page page,
                  SharedBrowsingState* shared = nullptr);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    // Select-all and focus the address bar (persistent mode only; no-op in modal).
    void focus_address_bar();

    // Navigate the inner BrowserView to url (validates first; no-op on bad URL).
    void navigate(std::string_view url);
    void navigate_back();
    void navigate_forward();
    void reload();

    // Called from Application::idle() to animate spinner or apply a completed load.
    void tick_if_loading();

    // Returns the URL of the currently displayed page.
    [[nodiscard]] std::string_view current_url() const { return view_->page().url; }

private:
    AddressBarMode mode_;
    BrowserView* view_;             // non-owning pointer into child list
    TInputLine* bar_{nullptr};      // non-owning; null in Modal mode
    TScrollBar* vscroll_{nullptr};  // non-owning; null until constructed

    void reposition(const TRect& inner);

    // URL-bar Tab-completion state.
    std::string completion_prefix_;
    std::string completion_current_;
    std::vector<std::string> completion_candidates_;
    std::size_t completion_idx_ = 0;
    bool completion_valid_ = false;

    void handle_url_completion(bool reverse);
};

}  // namespace tvshow::app
