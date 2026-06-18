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

    static auto initStatusLine(TRect r) -> TStatusLine*;
    static auto initMenuBar(TRect r) -> TMenuBar*;

    // Loads `url` ("file://" or "http(s)://") and opens it in a new BrowserWindow.
    // HTTP/network errors render an internal error page (SPEC §15.2); a
    // bad local path or unparseable HTML logs to stderr and is a no-op.
    void open_url(std::string_view url);

private:
    AddressBarMode mode_;
    SharedBrowsingState shared_browsing_state_;  // shared across all tabs

    // Returns the focused BrowserWindow in the desktop, or nullptr.
    static BrowserWindow* active_browser_window();
    static void show_window_list();
};

}  // namespace tvshow::app
