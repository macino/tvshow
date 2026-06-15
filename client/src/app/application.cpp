#include "tvshow/app/application.hpp"

#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#include <tvision/tv.h>

namespace tvshow::app {

Application::Application()
    : TProgInit(&Application::initStatusLine, &Application::initMenuBar,
                &Application::initDeskTop) {}

auto Application::initStatusLine(TRect r) -> TStatusLine* {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
                                  *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                                  *new TStatusItem(nullptr, kbF10, cmMenu));
}

auto Application::initMenuBar(TRect r) -> TMenuBar* {
    r.b.y = r.a.y + 1;
    return new TMenuBar(r, *new TSubMenu("~F~ile", kbAltF) +
                               *new TMenuItem("E~x~it", cmQuit, cmQuit, hcNoContext, "Alt-X"));
}

}  // namespace tvshow::app
