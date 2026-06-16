#include "tvshow/layout/form_focus.hpp"

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/form.hpp"

#include <vector>

namespace tvshow::layout {

namespace {

// NOLINTNEXTLINE(misc-no-recursion)
void collect_rec(const Box& box, std::vector<FormFocus>& out) {
    if (box.node != nullptr && box.node->node != nullptr) {
        const FormControlKind kind = form_control_kind(*box.node->node);
        if (kind != FormControlKind::None && kind != FormControlKind::Hidden) {
            out.push_back({box.node->node, kind, box.border_box});
        }
    }
    for (const auto& child : box.children) {
        collect_rec(child, out);
    }
}

}  // namespace

std::vector<FormFocus> collect_form_controls(const Box& root) {
    std::vector<FormFocus> out;
    collect_rec(root, out);
    return out;
}

}  // namespace tvshow::layout
