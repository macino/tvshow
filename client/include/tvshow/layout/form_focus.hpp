#pragma once

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/form.hpp"
#include "tvshow/layout/types.hpp"

#include <vector>

namespace tvshow::layout {

// One focusable form control (SPEC §13.2): its DOM node, kind, and bounding
// rect for focus-highlight via render::apply_focus.
struct FormFocus {
    const dom::Node* node = nullptr;
    FormControlKind kind = FormControlKind::None;
    CellRect span = {};
};

// Walks the box tree and returns all visible form controls in reading order.
[[nodiscard]] std::vector<FormFocus> collect_form_controls(const Box& root);

}  // namespace tvshow::layout
