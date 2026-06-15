#pragma once

#include "tvshow/dom/node.hpp"
#include "tvshow/style/types.hpp"

#include <vector>

namespace tvshow::style {

struct StyledNode {
    const dom::Node* node = nullptr;  // non-owning, points into the DOM tree
    ComputedStyle style{};
    std::vector<StyledNode> children;
};

}  // namespace tvshow::style
