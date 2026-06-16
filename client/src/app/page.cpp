#include "tvshow/app/page.hpp"

#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/resolver.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::app {

std::optional<Page> load_page(std::string_view url, layout::Viewport vp) {
    constexpr std::string_view k_prefix = "file://";
    if (!url.starts_with(k_prefix)) {
        std::cerr << "tvshow: unsupported URL scheme (only file:// supported until M12): " << url
                  << '\n';
        return std::nullopt;
    }
    const std::string path(url.substr(k_prefix.size()));

    const std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "tvshow: cannot open file: " << path << '\n';
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    const std::string html = contents.str();

    auto doc = dom::parse(html);
    if (!doc) {
        std::cerr << "tvshow: failed to parse HTML: " << path << '\n';
        return std::nullopt;
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
        return std::nullopt;
    }

    Page page{std::string(url), std::move(*doc), std::move(*tree), {}};
    page.box = layout::layout(page.tree, vp);
    return page;
}

}  // namespace tvshow::app
