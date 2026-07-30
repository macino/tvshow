#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::dom {

enum class NodeKind : uint8_t { Element, Text };

struct Attr {
    std::string name;
    std::string value;
};

// A single DOM node. Both Element and Text nodes share this struct.
// kind determines which fields are meaningful.
struct Node {
    NodeKind kind = NodeKind::Text;
    // Element-only:
    std::string tag;
    std::vector<Attr> attrs;
    // Text-only:
    std::string text;
    // Common:
    std::vector<std::unique_ptr<Node>> children;

    // Returns the value of the named attribute, or empty string_view.
    [[nodiscard]] std::string_view attr(std::string_view name) const noexcept;
    // Returns true if the element's class attribute contains cls.
    [[nodiscard]] bool has_class(std::string_view cls) const noexcept;
};

struct Document {
    std::string title;
    std::string charset{"utf-8"};
    std::vector<std::string> stylesheet_hrefs;  // from <link rel="stylesheet" href="...">
    std::vector<std::string> inline_styles;     // from <style> text content
    std::unique_ptr<Node> root;                 // always NodeKind::Element, tag "html"

    [[nodiscard]] const Node* body() const noexcept;
    [[nodiscard]] Node* body() noexcept;
};

// adr-sandboxed-scripting: depth-first search for the Element whose `id`
// attribute matches. Returns nullptr if none does. Mutable overload lets a
// script handler write through to the live tree (relayout() re-derives
// layout/render from it without a re-parse -- see BrowserView::run_onclick).
[[nodiscard]] Node* find_by_id(Node& root, std::string_view id) noexcept;
[[nodiscard]] const Node* find_by_id(const Node& root, std::string_view id) noexcept;

// Depth-first walk collecting the text content of every
// `<script type="text/lua">` element in `doc`, concatenated in document
// order (each block separated by a newline). Empty if none. `<script>` is
// present in the tree like any other element (just display:none'd by the
// UA stylesheet) -- this needs no parser support, just a tree-walk.
[[nodiscard]] std::string collect_lua_scripts(const Document& doc);

// Depth-first walk for `<meta name="name" content="...">`; returns `content`
// of the first match, or empty if none. Used for tvshow-specific hints like
// `tvshow-window-size`/`tvshow-window-color` -- not standard HTML semantics.
[[nodiscard]] std::string_view find_meta_content(const Document& doc,
                                                 std::string_view name) noexcept;

}  // namespace tvshow::dom
