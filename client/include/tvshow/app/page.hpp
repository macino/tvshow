#pragma once

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace tvshow::app {

// One loaded document, bundling everything layout::Box::node transitively
// points into (Box -> StyledNode -> dom::Node) so the Box tree stays valid
// for as long as the Page does. Declaration order matters: box is declared
// last so it's destroyed first, before tree and doc — the reverse would
// leave Box::node dangling mid-teardown.
struct Page {
    std::string url;
    dom::Document doc;
    style::StyledNode tree;
    layout::Box box;
};

// Reads, parses, resolves, and lays out `url` (a "file://" URL) at `vp`.
// Returns nullopt on read/parse/resolve failure; logs the reason to stderr.
[[nodiscard]] std::optional<Page> load_page(std::string_view url, layout::Viewport vp);

}  // namespace tvshow::app
