#pragma once

#include "tvshow/dom/node.hpp"

#include <cstdint>

namespace tvshow::layout {

enum class FormControlKind : uint8_t {
    None,
    Text,      // <input type="text"> or <input> with no type
    Password,  // <input type="password">
    Checkbox,  // <input type="checkbox">
    Radio,     // <input type="radio">
    Submit,    // <input type="submit"> or <button>
    Hidden,    // <input type="hidden"> — display:none
    Textarea,  // <textarea>
    Select,    // <select>
};

// Returns the form control kind for a DOM element node.
// Returns None for non-elements and non-form-control elements.
[[nodiscard]] FormControlKind form_control_kind(const dom::Node& node) noexcept;

struct FormControlSize {
    int cols;  // total columns including chrome (brackets etc.)
    int rows;
};

// Default rendered dimensions for a form control, respecting HTML size/cols/rows attrs.
// Caller must verify form_control_kind(node) != None before calling.
[[nodiscard]] FormControlSize form_control_size(const dom::Node& node) noexcept;

}  // namespace tvshow::layout
