#include "tvshow/layout/form_focus.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/form.hpp"

#include <vector>

namespace tvshow::layout {

namespace {

// NOLINTNEXTLINE(misc-no-recursion)
void collect_rec(const Box& box, const dom::Node* current_form, std::vector<FormFocus>& out) {
    if (box.node != nullptr && box.node->node != nullptr) {
        const dom::Node* dom_node = box.node->node;
        if (dom_node->tag == "form") {
            current_form = dom_node;
        }
        const FormControlKind kind = form_control_kind(*dom_node);
        if (kind != FormControlKind::None && kind != FormControlKind::Hidden) {
            out.push_back({dom_node, current_form, kind, box.border_box});
        }
    }
    for (const auto& child : box.children) {
        collect_rec(child, current_form, out);
    }
}

}  // namespace

std::vector<FormFocus> collect_form_controls(const Box& root) {
    std::vector<FormFocus> out;
    collect_rec(root, nullptr, out);
    return out;
}

}  // namespace tvshow::layout
