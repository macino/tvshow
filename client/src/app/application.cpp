#include "tvshow/app/application.hpp"

#define Uses_TKeys
#define Uses_TMenuItem
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TSubMenu
#define Uses_TWindow
#include "tvshow/app/browser_view.hpp"
#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/style/resolver.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    constexpr std::string_view k_prefix = "file://";
    if (!url.starts_with(k_prefix)) {
        std::cerr << "tvshow: unsupported URL scheme (only file:// supported in M8): " << url
                  << '\n';
        return;
    }
    const std::string path(url.substr(k_prefix.size()));

    const std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "tvshow: cannot open file: " << path << '\n';
        return;
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    const std::string html = contents.str();

    auto doc = dom::parse(html);
    if (!doc) {
        std::cerr << "tvshow: failed to parse HTML: " << path << '\n';
        return;
    }

    std::vector<css::Stylesheet> sheets;
    for (const auto& css_text : doc->inline_styles) {
        if (auto sheet = css::parse(css_text)) {
            sheets.push_back(std::move(*sheet));
        }
    }

    auto tree = style::resolve(*doc, sheets);
    if (!tree) {
        std::cerr << "tvshow: failed to resolve styles: " << path << '\n';
        return;
    }

    const layout::Viewport vp{std::max(1, deskTop->size.x), std::max(1, deskTop->size.y)};
    const layout::Box box = layout::layout(*tree, vp);
    render::CharGrid grid = render::render(box);

    const TRect bounds = deskTop->getExtent();
    auto* win = new TWindow(bounds, path.c_str(), wnNoNumber);
    win->options |= ofTileable;
    TRect view_bounds = win->getExtent();
    view_bounds.grow(-1, -1);
    win->insert(new BrowserView(view_bounds, std::move(grid)));
    deskTop->insert(win);
}

}  // namespace tvshow::app
