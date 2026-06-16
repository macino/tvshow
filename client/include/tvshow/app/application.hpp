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

    // Loads `url` ("file://" or "http(s)://") and opens it in a new window.
    // HTTP/network errors render an internal error page (SPEC §15.2); a
    // bad local path or unparseable HTML logs to stderr and is a no-op.
    static void open_url(std::string_view url);
};

}  // namespace tvshow::app
