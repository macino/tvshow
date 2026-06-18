#include "tvshow/images/renderer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::images {

std::vector<std::string> AltTextRenderer::render(int cols, int rows, std::string_view alt,
                                                 std::string_view /*src*/) const {
    if (cols <= 0 || rows <= 0) {
        return {};
    }
    const std::string blank(static_cast<std::size_t>(cols), ' ');
    std::vector<std::string> result(static_cast<std::size_t>(rows), blank);

    // Row 0: "[alt]" truncated or padded to cols.
    std::string label = "[";
    label += alt;
    label += "]";
    if (static_cast<int>(label.size()) > cols) {
        label.resize(static_cast<std::size_t>(cols));
    }
    result[0] = label;
    result[0].resize(static_cast<std::size_t>(cols), ' ');
    return result;
}

}  // namespace tvshow::images
