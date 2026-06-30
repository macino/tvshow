#pragma once

#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#include "tvshow/app/browser_window.hpp"

#include <tvision/tv.h>

#include <string_view>

namespace tvshow::app {

class Application : public TApplication {
public:
    explicit Application(AddressBarMode mode = AddressBarMode::Modal);

    void handleEvent(TEvent& event) override;
    void idle() override;
    void shutDown() override;

    static auto initStatusLine(TRect r) -> TStatusLine*;
    static auto initMenuBar(TRect r) -> TMenuBar*;

    // Loads `url` ("file://" or "http(s)://") and opens it in a new BrowserWindow.
    // HTTP/network errors render an internal error page (SPEC §15.2); a
    // bad local path or unparseable HTML logs to stderr and is a no-op.
    void open_url(std::string_view url);

    // Set forced style globally (called from main() after config load, and from
    // the settings dialog after the user changes the style preference).
    void set_forced_style(ForcedStyle fs);

    // Expose shared state for the settings dialog.
    SharedBrowsingState& browsing_state() { return shared_browsing_state_; }

private:
    AddressBarMode mode_;
    SharedBrowsingState shared_browsing_state_;  // shared across all tabs
    int cascade_step_ = 0;  // incremented on each open_url() for window staggering

    // Returns the next cascaded window bounds (2 cols, 1 row offset per step).
    TRect next_window_bounds();

    // Returns the focused BrowserWindow in the desktop, or nullptr.
    static BrowserWindow* active_browser_window();
    static void show_window_list();
};

}  // namespace tvshow::app
