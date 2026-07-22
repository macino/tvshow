#pragma once

#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"

#include <cstdint>
#include <vector>

namespace tvshow::layout {

struct OverlayBox;

// A single box in the box tree. Corresponds to one visible element node.
// Text nodes and display:none elements do not generate boxes.
struct Box {
    const style::StyledNode* node = nullptr;
    CellRect border_box;   // outer rectangle including border cells (not margin)
    CellRect content_box;  // inner content area for placing children / text
    std::vector<Box> children;
    // Only populated on the root Box returned by layout(): every
    // position:fixed/sticky element in the document, flattened out of the
    // normal tree (see OverlayBox below for why).
    std::vector<OverlayBox> overlays;
};

// SPEC Q-29: position:fixed/sticky elements are pulled out of the normal
// box tree into a flat list on the root Box (Box::overlays above), since
// their paint position depends on the viewport / current scroll offset
// rather than their place in the document flow -- something only the view
// layer (BrowserView) knows about at paint time, not the pure layout stage.
enum class OverlayKind : uint8_t { Fixed, Sticky };

struct OverlayBox {
    // Laid out at local origin (0,0); render::render(box) yields exactly
    // this element's own content, ready to be composited (blit) elsewhere.
    Box box;
    // Where to paint it, in viewport-relative (screen) coordinates, when pinned.
    CellPos pinned_origin;
    OverlayKind kind = OverlayKind::Fixed;
    // Sticky only: the row it occupies in normal document flow (it stays in
    // the main tree too, unlike fixed). Once the current scroll offset would
    // scroll this row above pinned_origin.row, the view should paint the
    // overlay instead (pinned); before that, the in-flow copy is already
    // visible and no overlay paint is needed.
    int static_doc_row = 0;
};

// Hit-test: find the deepest box containing the given point.
// Returns nullptr if no box contains the point.
[[nodiscard]] const Box* hit_test(const Box& root, Point pt);

}  // namespace tvshow::layout
