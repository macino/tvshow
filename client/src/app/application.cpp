#include "tvshow/app/application.hpp"

#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#define Uses_TWindow
#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/layout/types.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <string_view>
#include <utility>

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

void Application::open_file_url(std::string_view url) {
    const layout::Viewport vp{std::max(1, deskTop->size.x), std::max(1, deskTop->size.y)};
    auto page = load_page(url, vp);
    if (!page) {
        return;
    }

    const TRect bounds = deskTop->getExtent();
    auto* win = new TWindow(bounds, std::string(url).c_str(), wnNoNumber);
    win->options |= ofTileable;
    TRect view_bounds = win->getExtent();
    view_bounds.grow(-1, -1);
    win->insert(new BrowserView(view_bounds, std::move(*page)));
    deskTop->insert(win);
}

}  // namespace tvshow::app
