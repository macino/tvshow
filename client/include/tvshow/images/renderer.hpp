#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tvshow::images {

// Decoded image pixel data (RGBA, row-major).
struct ImageData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
};

// Map from image src URL to decoded pixel data.
using ImageCache = std::unordered_map<std::string, ImageData>;

// Renders an image into a grid of character strings.
// Returns exactly `rows` strings each of exactly `cols` chars.
class ImageRenderer {
public:
    ImageRenderer() = default;
    ImageRenderer(const ImageRenderer&) = default;
    ImageRenderer(ImageRenderer&&) = default;
    ImageRenderer& operator=(const ImageRenderer&) = default;
    ImageRenderer& operator=(ImageRenderer&&) = default;
    virtual ~ImageRenderer() = default;

    [[nodiscard]] virtual std::vector<std::string> render(int cols, int rows, std::string_view alt,
                                                          std::string_view src) const = 0;
};

// Renders `[alt]` in the first row, spaces elsewhere.
class AltTextRenderer : public ImageRenderer {
public:
    [[nodiscard]] std::vector<std::string> render(int cols, int rows, std::string_view alt,
                                                  std::string_view src) const override;
};

// Renders images as Unicode braille patterns (U+2800..U+28FF).
// Each braille character represents a 2x4 pixel block. Pixels are
// thresholded to black/white by luminance.
class BrailleRenderer : public ImageRenderer {
public:
    explicit BrailleRenderer(const ImageCache* cache) : cache_(cache) {}

    [[nodiscard]] std::vector<std::string> render(int cols, int rows, std::string_view alt,
                                                  std::string_view src) const override;

private:
    const ImageCache* cache_;
};

}  // namespace tvshow::images
