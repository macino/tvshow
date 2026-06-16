#pragma once

#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#include <tvision/tv.h>

#include <string_view>

namespace tvshow::app {

class Application : public TApplication {
public:
    Application();

    static auto initStatusLine(TRect r) -> TStatusLine*;
    static auto initMenuBar(TRect r) -> TMenuBar*;

    // M8 scope: load a local file and open it in a new window, no HTTP yet.
    // `url` must be a "file://" URL. Logs to stderr and is a no-op on
    // failure (bad path, parse failure) — error pages land with M12.
    static void open_file_url(std::string_view url);
};

}  // namespace tvshow::app
