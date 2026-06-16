#include "tvshow/layout/form.hpp"

#include "tvshow/dom/node.hpp"

#include <charconv>
#include <string_view>

namespace tvshow::layout {

namespace {

int parse_int_attr(const dom::Node& node, std::string_view attr_name, int fallback) noexcept {
    const std::string_view val = node.attr(attr_name);
    if (val.empty()) {
        return fallback;
    }
    int result = fallback;
    std::from_chars(val.data(), val.data() + val.size(), result);
    return (result > 0) ? result : fallback;
}

}  // namespace

FormControlKind form_control_kind(const dom::Node& node) noexcept {
    if (node.kind != dom::NodeKind::Element) {
        return FormControlKind::None;
    }
    if (node.tag == "textarea") {
        return FormControlKind::Textarea;
    }
    if (node.tag == "select") {
        return FormControlKind::Select;
    }
    if (node.tag == "button") {
        return FormControlKind::Submit;
    }
    if (node.tag != "input") {
        return FormControlKind::None;
    }
    const std::string_view type = node.attr("type");
    if (type == "password") {
        return FormControlKind::Password;
    }
    if (type == "checkbox") {
        return FormControlKind::Checkbox;
    }
    if (type == "radio") {
        return FormControlKind::Radio;
    }
    if (type == "submit") {
        return FormControlKind::Submit;
    }
    if (type == "hidden") {
        return FormControlKind::Hidden;
    }
    // "text", "" (unset), or any unrecognised type → text input.
    return FormControlKind::Text;
}

FormControlSize form_control_size(const dom::Node& node) noexcept {
    const FormControlKind kind = form_control_kind(node);
    switch (kind) {
    case FormControlKind::None:
        return {0, 0};
    case FormControlKind::Text:
    case FormControlKind::Password: {
        // size attr (default 20) is the visible text width; add 2 for [ ].
        const int inner = parse_int_attr(node, "size", 20);
        return {inner + 2, 1};
    }
    case FormControlKind::Checkbox:
    case FormControlKind::Radio:
        return {3, 1};
    case FormControlKind::Submit: {
        const std::string_view label = node.attr("value");
        const int inner = static_cast<int>(label.empty() ? 6 : label.size());
        return {inner + 3, 1};  // "[ label]" — bracket, space, label, bracket
    }
    case FormControlKind::Hidden:
        return {0, 0};
    case FormControlKind::Textarea: {
        const int cols = parse_int_attr(node, "cols", 40);
        const int rows = parse_int_attr(node, "rows", 5);
        return {cols + 2, rows + 2};  // +2 for border on each axis
    }
    case FormControlKind::Select: {
        const int inner = parse_int_attr(node, "size", 12);
        return {inner + 5, 1};  // "[ val  ▾ ]"
    }
    }
    return {0, 0};
}

}  // namespace tvshow::layout
