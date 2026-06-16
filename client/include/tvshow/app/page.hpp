#pragma once

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace tvshow::app {

// One loaded document, bundling everything layout::Box::node transitively
// points into (Box -> StyledNode -> dom::Node) so the Box tree stays valid
// for as long as the Page does. tree is heap-allocated (not stored by
// value) so its address survives Page being moved — e.g. through
// std::optional<Page>'s converting constructor in load_page's return —
// which would otherwise dangle every Box::node pointing at the root.
// Declaration order also matters: box is declared last so it's destroyed
// first, before tree and doc — the reverse would leave Box::node dangling
// mid-teardown.
struct Page {
    std::string url;
    dom::Document doc;
    std::unique_ptr<style::StyledNode> tree;
    layout::Box box;
};

// Reads, parses, resolves, and lays out `url` (a "file://" URL) at `vp`.
// Returns nullopt on read/parse/resolve failure; logs the reason to stderr.
[[nodiscard]] std::optional<Page> load_page(std::string_view url, layout::Viewport vp);

}  // namespace tvshow::app
