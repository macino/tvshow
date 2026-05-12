#pragma once

#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDeskTop
#include <tvision/tv.h>

namespace tvshow::app {

class Application : public TApplication {
public:
    Application();

    static auto initStatusLine(TRect r) -> TStatusLine*;
    static auto initMenuBar(TRect r) -> TMenuBar*;
};

}  // namespace tvshow::app
