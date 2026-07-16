#include "tvshow/images/decode.hpp"

#include "tvshow/images/renderer.hpp"

#include "stb_image.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace tvshow::images {

std::optional<ImageData> decode_image(std::string_view bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(bytes.data()),  // NOLINT
        static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (pixels == nullptr || w <= 0 || h <= 0) {
        return std::nullopt;
    }
    ImageData img;
    img.width = w;
    img.height = h;
    img.pixels.assign(pixels, pixels + (static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4));
    stbi_image_free(pixels);
    return img;
}

}  // namespace tvshow::images
