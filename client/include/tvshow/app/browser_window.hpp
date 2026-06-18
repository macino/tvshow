#pragma once

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TInputLine
#define Uses_TWindow
#include <tvision/tv.h>

#include <cstdint>
#include <string_view>

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
    BrowserWindow(const TRect& bounds, AddressBarMode mode, Page page);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    // Select-all and focus the address bar (persistent mode only; no-op in modal).
    void focus_address_bar();

    // Navigate the inner BrowserView to url (validates first; no-op on bad URL).
    void navigate(std::string_view url);
    void navigate_back();
    void navigate_forward();
    void reload();

    // Returns the URL of the currently displayed page.
    [[nodiscard]] std::string_view current_url() const { return view_->page().url; }

private:
    AddressBarMode mode_;
    BrowserView* view_;         // non-owning pointer into child list
    TInputLine* bar_{nullptr};  // non-owning; null in Modal mode

    void reposition(const TRect& inner);
};

}  // namespace tvshow::app
