#include "tvshow/dom/node.hpp"

#include <string>
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

namespace {

// NOLINTNEXTLINE(misc-no-recursion)
const Node* find_by_id_impl(const Node& node, std::string_view id) noexcept {
    if (node.kind == NodeKind::Element && node.attr("id") == id) {
        return &node;
    }
    for (const auto& child : node.children) {
        if (!child) {
            continue;
        }
        if (const Node* found = find_by_id_impl(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_lua_scripts_impl(const Node& node, std::string& out) {
    if (node.kind == NodeKind::Element && node.tag == "script" &&
        node.attr("type") == "text/lua") {
        for (const auto& child : node.children) {
            if (child && child->kind == NodeKind::Text) {
                out += child->text;
            }
        }
        out += '\n';
        return;  // no renderable/scriptable content below a <script>
    }
    for (const auto& child : node.children) {
        if (child) {
            collect_lua_scripts_impl(*child, out);
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
const Node* find_meta_impl(const Node& node, std::string_view name) noexcept {
    if (node.kind == NodeKind::Element && node.tag == "meta" && node.attr("name") == name) {
        return &node;
    }
    for (const auto& child : node.children) {
        if (!child) {
            continue;
        }
        if (const Node* found = find_meta_impl(*child, name)) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

Node* find_by_id(Node& root, std::string_view id) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return const_cast<Node*>(find_by_id_impl(root, id));
}

const Node* find_by_id(const Node& root, std::string_view id) noexcept {
    return find_by_id_impl(root, id);
}

std::string collect_lua_scripts(const Document& doc) {
    std::string out;
    if (doc.root) {
        collect_lua_scripts_impl(*doc.root, out);
    }
    return out;
}

std::string_view find_meta_content(const Document& doc, std::string_view name) noexcept {
    if (!doc.root) {
        return {};
    }
    const Node* meta = find_meta_impl(*doc.root, name);
    return meta ? meta->attr("content") : std::string_view{};
}

}  // namespace tvshow::dom
