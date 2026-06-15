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

}  // namespace tvshow::dom
