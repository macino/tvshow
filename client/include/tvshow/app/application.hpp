#pragma once

#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#include "tvshow/app/browser_window.hpp"

#include <tvision/tv.h>

#include <memory>
#include <string>
#include <string_view>

namespace tvshow::net {
class ExtensionServer;
}

namespace tvshow::app {

class Application : public TApplication {
public:
    explicit Application(AddressBarMode mode = AddressBarMode::Modal);
    // Declared (not defaulted) because ~ExtensionServer needs a complete
    // type -- extension_server_hpp is only forward-declared here.
    ~Application() override;

    void handleEvent(TEvent& event) override;
    void idle() override;
    void shutDown() override;

    static auto initStatusLine(TRect r) -> TStatusLine*;
    static auto initMenuBar(TRect r) -> TMenuBar*;

    // Loads `url` ("file://" or "http(s)://") and opens it in a new BrowserWindow.
    // HTTP/network errors render an internal error page (SPEC §15.2); a
    // bad local path or unparseable HTML logs to stderr and is a no-op.
    void open_url(std::string_view url);

    // Sets the app-wide default forced style: seeds every new window opened
    // from now on, and re-applies live to every window already open.
    // Called from main() after config load, and from the settings dialog
    // after the user changes the persistent style preference.
    void set_forced_style(ForcedStyle fs);

    // View > Style: overrides the forced style for the active window only —
    // doesn't touch the app-wide default or any other open tab.
    void set_active_window_style(ForcedStyle fs);

    // Expose shared state for the settings dialog.
    SharedBrowsingState& browsing_state() { return shared_browsing_state_; }

private:
    AddressBarMode mode_;
    SharedBrowsingState shared_browsing_state_;  // shared across all tabs
    int cascade_step_ = 0;  // incremented on each open_url() for window staggering
    // URL queued by cmOpenUrl/cmNewTab to be opened on the next idle() tick, after
    // the modal URL picker dialog has fully unwound from the tvision event stack.
    std::string deferred_open_url_;

    // Returns the next cascaded window bounds (2 cols, 1 row offset per step).
    TRect next_window_bounds();
    // adr-native-demo-windows: fixed-size (w x h) window, centered on the
    // desktop with the same per-open stagger next_window_bounds() uses --
    // for content-sized demo windows that shouldn't inherit BrowserWindow's
    // fill-the-desktop sizing.
    TRect fixed_window_bounds(int w, int h);

    // Returns the focused BrowserWindow in the desktop, or nullptr.
    static BrowserWindow* active_browser_window();
    static void show_window_list();

    // adr-extension-server: starts the internal extension HTTP server on
    // first use (no-op if already running). Returns its bound port, or 0
    // if it failed to bind.
    std::unique_ptr<net::ExtensionServer> extension_server_;
    int ensure_extension_server();
};

}  // namespace tvshow::app
