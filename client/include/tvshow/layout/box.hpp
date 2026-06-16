#pragma once

#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"

#include <vector>

namespace tvshow::layout {

// A single box in the box tree. Corresponds to one visible element node.
// Text nodes and display:none elements do not generate boxes.
struct Box {
    const style::StyledNode* node = nullptr;
    CellRect border_box;   // outer rectangle including border cells (not margin)
    CellRect content_box;  // inner content area for placing children / text
    std::vector<Box> children;
};

}  // namespace tvshow::layout
