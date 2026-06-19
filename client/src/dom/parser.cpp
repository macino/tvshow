#include "tvshow/dom/parser.hpp"

#include "tvshow/dom/node.hpp"

#include <gumbo.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::dom {

namespace {

// Wrappers for Gumbo's C union accesses — isolated so NOLINT is not scattered.
const GumboElement& gumbo_elem(const GumboNode& n) noexcept {
    return n.v.element;  // NOLINT(cppcoreguidelines-pro-type-union-access)
}
const GumboText& gumbo_text(const GumboNode& n) noexcept {
    return n.v.text;  // NOLINT(cppcoreguidelines-pro-type-union-access)
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string_view trim(std::string_view s) noexcept {
    const auto start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string_view::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}

// Forward declaration for mutual recursion.
// NOLINTNEXTLINE(misc-no-recursion)
std::unique_ptr<Node> convert_node(const GumboNode* gn);

// NOLINTNEXTLINE(misc-no-recursion)
void convert_children(const GumboVector& gumbo_children, std::vector<std::unique_ptr<Node>>& out) {
    for (unsigned i = 0; i < gumbo_children.length; ++i) {
        const auto* child = static_cast<const GumboNode*>(gumbo_children.data[i]);
        auto node = convert_node(child);
        if (node != nullptr)
            out.push_back(std::move(node));
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
std::unique_ptr<Node> convert_element(const GumboNode* gn) {
    auto node = std::make_unique<Node>();
    node->kind = NodeKind::Element;

    const GumboElement& elem = gumbo_elem(*gn);
    node->tag = to_lower(gumbo_normalized_tagname(elem.tag));
    if (node->tag.empty() && elem.original_tag.length > 0) {
        std::string raw(elem.original_tag.data, elem.original_tag.length);
        const auto lt = raw.find('<');
        if (lt != std::string::npos)
            raw = raw.substr(lt + 1);
        const auto space = raw.find_first_of(" \t\n>/");
        if (space != std::string::npos)
            raw = raw.substr(0, space);
        node->tag = to_lower(raw);
    }

    for (unsigned i = 0; i < elem.attributes.length; ++i) {
        const auto* ga = static_cast<const GumboAttribute*>(elem.attributes.data[i]);
        Attr a;
        a.name = to_lower(std::string(ga->name));
        a.value = std::string(ga->value);
        node->attrs.push_back(std::move(a));
    }

    convert_children(elem.children, node->children);
    return node;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::unique_ptr<Node> convert_node(const GumboNode* gn) {
    if (gn == nullptr)
        return nullptr;

    switch (gn->type) {
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE:
        return convert_element(gn);

    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
    case GUMBO_NODE_WHITESPACE: {
        auto node = std::make_unique<Node>();
        node->kind = NodeKind::Text;
        node->text = std::string(gumbo_text(*gn).text);
        return node;
    }

    default:
        return nullptr;
    }
}

// --- Head metadata collection ---

void handle_title(const GumboElement& elem, Document& doc) {
    for (unsigned i = 0; i < elem.children.length; ++i) {
        const auto* child = static_cast<const GumboNode*>(elem.children.data[i]);
        if (child != nullptr &&
            (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE)) {
            doc.title += gumbo_text(*child).text;
        }
    }
}

void handle_meta(const GumboElement& elem, Document& doc) {
    const GumboAttribute* charset_attr = gumbo_get_attribute(&elem.attributes, "charset");
    if (charset_attr != nullptr && charset_attr->value != nullptr &&
        charset_attr->value[0] != '\0') {
        doc.charset = to_lower(std::string(trim(charset_attr->value)));
        return;
    }
    const GumboAttribute* equiv = gumbo_get_attribute(&elem.attributes, "http-equiv");
    if (equiv == nullptr)
        return;
    if (to_lower(std::string(trim(equiv->value))) != "content-type")
        return;
    const GumboAttribute* content = gumbo_get_attribute(&elem.attributes, "content");
    if (content == nullptr)
        return;
    const std::string_view ct(content->value);
    const auto pos = ct.find("charset=");
    if (pos != std::string_view::npos)
        doc.charset = to_lower(std::string(trim(ct.substr(pos + 8))));
}

void handle_link(const GumboElement& elem, Document& doc) {
    const GumboAttribute* rel = gumbo_get_attribute(&elem.attributes, "rel");
    if (rel == nullptr)
        return;
    if (to_lower(std::string(trim(rel->value))) != "stylesheet")
        return;
    const GumboAttribute* href = gumbo_get_attribute(&elem.attributes, "href");
    if (href != nullptr && href->value != nullptr && href->value[0] != '\0')
        doc.stylesheet_hrefs.emplace_back(href->value);
}

void handle_style(const GumboElement& elem, Document& doc) {
    std::string css;
    for (unsigned i = 0; i < elem.children.length; ++i) {
        const auto* child = static_cast<const GumboNode*>(elem.children.data[i]);
        if (child != nullptr &&
            (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE ||
             child->type == GUMBO_NODE_CDATA)) {
            css += gumbo_text(*child).text;
        }
    }
    if (!css.empty())
        doc.inline_styles.push_back(std::move(css));
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_head_metadata(const GumboNode* gn, Document& doc) {
    if (gn == nullptr || gn->type != GUMBO_NODE_ELEMENT)
        return;

    const GumboElement& elem = gumbo_elem(*gn);

    switch (elem.tag) {
    case GUMBO_TAG_TITLE:
        handle_title(elem, doc);
        break;
    case GUMBO_TAG_META:
        handle_meta(elem, doc);
        break;
    case GUMBO_TAG_LINK:
        handle_link(elem, doc);
        break;
    case GUMBO_TAG_STYLE:
        handle_style(elem, doc);
        break;
    default:
        for (unsigned i = 0; i < elem.children.length; ++i) {
            collect_head_metadata(static_cast<const GumboNode*>(elem.children.data[i]), doc);
        }
        break;
    }
}

// Collect <style> elements from the body subtree (pages commonly embed
// <style> blocks outside <head>; they are valid CSS cascade participants).
// NOLINTNEXTLINE(misc-no-recursion)
void collect_body_styles(const GumboNode* gn, Document& doc) {
    if (gn == nullptr || gn->type != GUMBO_NODE_ELEMENT)
        return;
    const GumboElement& elem = gumbo_elem(*gn);
    if (elem.tag == GUMBO_TAG_STYLE) {
        handle_style(elem, doc);
        return;
    }
    for (unsigned i = 0; i < elem.children.length; ++i) {
        collect_body_styles(static_cast<const GumboNode*>(elem.children.data[i]), doc);
    }
}

void walk_head(const GumboNode* html_node, Document& doc) {
    if (html_node == nullptr || html_node->type != GUMBO_NODE_ELEMENT)
        return;
    const GumboElement& html_elem = gumbo_elem(*html_node);
    for (unsigned i = 0; i < html_elem.children.length; ++i) {
        const auto* child = static_cast<const GumboNode*>(html_elem.children.data[i]);
        if (child == nullptr || child->type != GUMBO_NODE_ELEMENT)
            continue;
        const GumboTag tag = gumbo_elem(*child).tag;
        if (tag == GUMBO_TAG_HEAD) {
            const GumboElement& head = gumbo_elem(*child);
            for (unsigned j = 0; j < head.children.length; ++j) {
                collect_head_metadata(static_cast<const GumboNode*>(head.children.data[j]), doc);
            }
        } else if (tag == GUMBO_TAG_BODY) {
            collect_body_styles(child, doc);
        }
    }
}

}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::optional<Document> parse(std::string_view html, std::string_view charset) {
    GumboOutput* output = gumbo_parse_with_options(&kGumboDefaultOptions, html.data(), html.size());
    if (output == nullptr)
        return std::nullopt;

    Document doc;
    doc.charset = std::string(charset);

    const GumboNode* gumbo_root = output->root;
    if (gumbo_root != nullptr && gumbo_root->type == GUMBO_NODE_ELEMENT) {
        walk_head(gumbo_root, doc);
        doc.root = convert_element(gumbo_root);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return doc;
}

}  // namespace tvshow::dom
