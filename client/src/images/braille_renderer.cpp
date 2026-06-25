#include "tvshow/images/renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::images {

namespace {

// Braille dot layout for a 2x4 block mapped to Unicode U+2800..U+28FF:
// Col 0  Col 1
// Dot 0  Dot 3   (row 0)
// Dot 1  Dot 4   (row 1)
// Dot 2  Dot 5   (row 2)
// Dot 6  Dot 7   (row 3)
constexpr uint8_t k_dot_bits[4][2] = {
    {0x01, 0x08},  // row 0
    {0x02, 0x10},  // row 1
    {0x04, 0x20},  // row 2
    {0x40, 0x80},  // row 3
};

constexpr uint8_t k_luma_threshold = 128;

uint8_t luminance(uint8_t r, uint8_t g, uint8_t b) noexcept {
    return static_cast<uint8_t>((static_cast<unsigned>(r) * 77U +
                                 static_cast<unsigned>(g) * 150U +
                                 static_cast<unsigned>(b) * 29U) >> 8U);
}

// Encode a single UTF-8 codepoint into a string.
void append_utf8(std::string& out, char32_t cp) {
    if (cp < 0x80U) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800U) {
        out += static_cast<char>(0xC0U | (cp >> 6U));
        out += static_cast<char>(0x80U | (cp & 0x3FU));
    } else if (cp < 0x10000U) {
        out += static_cast<char>(0xE0U | (cp >> 12U));
        out += static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (cp & 0x3FU));
    }
}

}  // namespace

std::vector<std::string> BrailleRenderer::render(int cols, int rows, std::string_view alt,
                                                  std::string_view src) const {
    if (cols <= 0 || rows <= 0) {
        return {};
    }

    // Fall back to alt text if no cached image data.
    if (cache_ == nullptr) {
        return AltTextRenderer{}.render(cols, rows, alt, src);
    }
    const auto it = cache_->find(std::string(src));
    if (it == cache_->end() || it->second.pixels.empty()) {
        return AltTextRenderer{}.render(cols, rows, alt, src);
    }

    const auto& img = it->second;
    const int img_w = img.width;
    const int img_h = img.height;

    // Each braille char covers a 2x4 pixel block.
    // Scale image to fit cols*2 x rows*4 pixel grid.
    const int grid_px_w = cols * 2;
    const int grid_px_h = rows * 4;

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(rows));

    for (int cr = 0; cr < rows; ++cr) {
        std::string line;
        for (int cc = 0; cc < cols; ++cc) {
            uint8_t pattern = 0;
            for (int dy = 0; dy < 4; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int px = cc * 2 + dx;
                    const int py = cr * 4 + dy;
                    // Map grid pixel to source image pixel.
                    const int sx = img_w * px / grid_px_w;
                    const int sy = img_h * py / grid_px_h;
                    const int si = std::clamp(sy, 0, img_h - 1) * img_w +
                                   std::clamp(sx, 0, img_w - 1);
                    const size_t idx = static_cast<size_t>(si) * 4;
                    const uint8_t r = img.pixels[idx];
                    const uint8_t g = img.pixels[idx + 1];
                    const uint8_t b = img.pixels[idx + 2];
                    if (luminance(r, g, b) < k_luma_threshold) {
                        pattern |= k_dot_bits[dy][dx];
                    }
                }
            }
            append_utf8(line, static_cast<char32_t>(0x2800U + pattern));
        }
        result.push_back(std::move(line));
    }
    return result;
}

}  // namespace tvshow::images
