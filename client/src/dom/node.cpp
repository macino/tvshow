#include "tvshow/dom/node.hpp"

#include <string_view>

namespace tvshow::dom {

std::string_view Node::attr(std::string_view name) const noexcept {
    for (const auto& a : attrs) {
        if (a.name == name)
            return a.value;
    }
    return {};
}

bool Node::has_class(std::string_view cls) const noexcept {
    const std::string_view classes = attr("class");
    if (classes.empty())
        return false;

    std::string_view remaining = classes;
    while (!remaining.empty()) {
        const auto space = remaining.find_first_of(" \t\n\r\f");
        const auto token = remaining.substr(0, space);
        if (token == cls)
            return true;
        if (space == std::string_view::npos)
            break;
        remaining = remaining.substr(space + 1);
    }
    return false;
}

namespace {

const Node* find_child_tag(const Node& node, std::string_view tag) noexcept {
    for (const auto& child : node.children) {
        if (child && child->kind == NodeKind::Element && child->tag == tag)
            return child.get();
    }
    return nullptr;
}

}  // namespace

const Node* Document::body() const noexcept {
    if (!root)
        return nullptr;
    return find_child_tag(*root, "body");
}

Node* Document::body() noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return const_cast<Node*>(const_cast<const Document*>(this)->body());
}

}  // namespace tvshow::dom
