#include "tvshow/layout/form_data.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/form.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tvshow::layout {

namespace {

using TextMap = std::unordered_map<const dom::Node*, std::string>;
using CheckMap = std::unordered_map<const dom::Node*, bool>;

std::string_view live_text(const dom::Node& node, const TextMap& text_values) noexcept {
    const auto it = text_values.find(&node);
    return (it != text_values.end()) ? std::string_view(it->second) : node.attr("value");
}

bool live_checked(const dom::Node& node, const CheckMap& checked_values) noexcept {
    const auto it = checked_values.find(&node);
    return (it != checked_values.end()) ? it->second : (node.attr("checked").data() != nullptr);
}

std::string textarea_value(const dom::Node& node, const TextMap& text_values) {
    const auto it = text_values.find(&node);
    if (it != text_values.end()) {
        return it->second;
    }
    std::string val;
    for (const auto& child : node.children) {
        if (child && child->kind == dom::NodeKind::Text) {
            val += child->text;
        }
    }
    return val;
}

void collect_control(const dom::Node& node, const TextMap& text_vals, const CheckMap& check_vals,
                     std::vector<FormField>& out) {
    const std::string_view name = node.attr("name");
    const FormControlKind kind = form_control_kind(node);
    switch (kind) {
    case FormControlKind::Text:
    case FormControlKind::Password:
        out.push_back({std::string(name), std::string(live_text(node, text_vals))});
        return;
    case FormControlKind::Hidden:
        out.push_back({std::string(name), std::string(node.attr("value"))});
        return;
    case FormControlKind::Checkbox:
    case FormControlKind::Radio:
        if (live_checked(node, check_vals)) {
            const std::string_view attr_val = node.attr("value");
            out.push_back({std::string(name), attr_val.empty() ? "on" : std::string(attr_val)});
        }
        return;
    case FormControlKind::Textarea:
        out.push_back({std::string(name), textarea_value(node, text_vals)});
        return;
    case FormControlKind::Select: {
        const auto it = text_vals.find(&node);
        if (it != text_vals.end()) {
            out.push_back({std::string(name), it->second});
        }
        return;
    }
    case FormControlKind::File:  // collected separately by collect_form_files (SPEC Q-28)
    case FormControlKind::Submit:
    case FormControlKind::None:
        return;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_files(const dom::Node& node, const TextMap& text_vals,
                   std::vector<FormFileRef>& out) {
    if (node.kind != dom::NodeKind::Element) {
        return;
    }
    const FormControlKind kind = form_control_kind(node);
    if (kind == FormControlKind::File) {
        const std::string_view name = node.attr("name");
        const auto it = text_vals.find(&node);
        if (!name.empty() && it != text_vals.end() && !it->second.empty()) {
            out.push_back({std::string(name), it->second});
        }
        return;
    }
    if (kind != FormControlKind::None) {
        return;  // don't recurse into other controls' children
    }
    for (const auto& child : node.children) {
        if (child) {
            collect_files(*child, text_vals, out);
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_controls(const dom::Node& node, const TextMap& text_vals, const CheckMap& check_vals,
                      std::vector<FormField>& out) {
    if (node.kind != dom::NodeKind::Element) {
        return;
    }
    const std::string_view name = node.attr("name");
    const FormControlKind kind = form_control_kind(node);
    if (kind != FormControlKind::None && !name.empty()) {
        collect_control(node, text_vals, check_vals, out);
        return;  // Don't recurse into a control's children.
    }
    for (const auto& child : node.children) {
        if (child) {
            collect_controls(*child, text_vals, check_vals, out);
        }
    }
}

constexpr bool is_unreserved(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '_' || c == '.' || c == '~';
}

}  // namespace

std::vector<FormField> collect_form_fields(const dom::Node& form, const TextMap& text_values,
                                           const CheckMap& checked_values) {
    std::vector<FormField> fields;
    for (const auto& child : form.children) {
        if (child) {
            collect_controls(*child, text_values, checked_values, fields);
        }
    }
    return fields;
}

std::vector<FormFileRef> collect_form_files(const dom::Node& form, const TextMap& text_values) {
    std::vector<FormFileRef> files;
    for (const auto& child : form.children) {
        if (child) {
            collect_files(*child, text_values, files);
        }
    }
    return files;
}

std::string url_encode(std::string_view s) {
    static constexpr std::string_view k_hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) {
        if (c == ' ') {
            out += '+';
        } else if (is_unreserved(c)) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += k_hex[(c >> 4U) & 0x0FU];
            out += k_hex[c & 0x0FU];
        }
    }
    return out;
}

std::string encode_fields(const std::vector<FormField>& fields) {
    std::string out;
    for (const auto& f : fields) {
        if (!out.empty()) {
            out += '&';
        }
        out += url_encode(f.name);
        out += '=';
        out += url_encode(f.value);
    }
    return out;
}

std::string encode_multipart(const std::vector<FormField>& fields,
                             const std::vector<FormFilePart>& files, std::string_view boundary) {
    std::string out;
    for (const auto& f : fields) {
        out += "--";
        out += boundary;
        out += "\r\n";
        out += "Content-Disposition: form-data; name=\"" + f.name + "\"\r\n\r\n";
        out += f.value;
        out += "\r\n";
    }
    for (const auto& file : files) {
        out += "--";
        out += boundary;
        out += "\r\n";
        out += "Content-Disposition: form-data; name=\"" + file.field_name + "\"; filename=\"" +
               file.filename + "\"\r\n";
        out += "Content-Type: " + file.content_type + "\r\n\r\n";
        out += file.content;
        out += "\r\n";
    }
    out += "--";
    out += boundary;
    out += "--\r\n";
    return out;
}

}  // namespace tvshow::layout
