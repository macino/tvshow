#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tvshow::images {

// Renders an image into a grid of character strings.
// Returns exactly `rows` strings each of exactly `cols` chars.
// v1 concrete: AltTextRenderer (renders `[alt]`).
// Future: sixel/kitty renderer via the same interface.
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

}  // namespace tvshow::images
