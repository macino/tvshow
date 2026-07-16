#pragma once

#include "tvshow/images/renderer.hpp"

#include <optional>
#include <string_view>

namespace tvshow::images {

// Decodes image bytes (PNG/JPEG/BMP/GIF via stb_image, ADR-004) into RGBA
// pixel data. Pure -- bytes must already be in memory, no I/O here.
// Returns nullopt on unsupported or corrupt data.
[[nodiscard]] std::optional<ImageData> decode_image(std::string_view bytes);

}  // namespace tvshow::images
