#pragma once

#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/images/renderer.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/cookie_jar.hpp"
#include "tvshow/style/tree.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
    std::vector<css::Stylesheet> sheets;
    std::unique_ptr<style::StyledNode> tree;
    layout::Box box;
    bool fallback = false;
    images::ImageCache images;  // populated only when load_page's fetch_images is true
};

// Reads, parses, resolves, and lays out `url` at `vp`.
// Returns nullopt on read/parse/resolve failure; logs the reason.
// jar may be null (cookies disabled).
// When skip_external_css is true, external <link> stylesheets are not fetched
// (useful when a forced theme will override author CSS anyway).
// When fetch_images is true, every <img src> is fetched and decoded (ADR-004)
// into Page::images; left false, images.empty() and BrailleRenderer falls
// back to alt text. Off by default: avoids the extra network/decode cost
// when the alt-text renderer is in use.
[[nodiscard]] std::optional<Page> load_page(std::string_view url, layout::Viewport vp,
                                            net::CookieJar* jar = nullptr,
                                            bool skip_external_css = false,
                                            bool fetch_images = false);

// Builds a Page containing an error HTML document (for exception fallbacks).
// Does not return nullopt; any internal failure falls back to a minimal grid.
[[nodiscard]] std::optional<Page> load_page_from_error(std::string_view title,
                                                       std::string_view detail,
                                                       layout::Viewport vp);

// Issues a POST to action_url with application/x-www-form-urlencoded body,
// then parses and lays out the response document.
// jar may be null (cookies disabled).
[[nodiscard]] std::optional<Page> post_page(std::string_view action_url, std::string_view body,
                                            layout::Viewport vp,
                                            net::CookieJar* jar = nullptr);

}  // namespace tvshow::app
