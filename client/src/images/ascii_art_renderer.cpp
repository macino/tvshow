#include "tvshow/images/renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::images {

namespace {

// Dark -> light luminance ramp, 10 levels (adr-ascii-art-renderer).
constexpr std::array<char, 10> k_ramp = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};

uint8_t luminance(uint8_t r, uint8_t g, uint8_t b) noexcept {
    return static_cast<uint8_t>((static_cast<unsigned>(r) * 77U +
                                 static_cast<unsigned>(g) * 150U +
                                 static_cast<unsigned>(b) * 29U) >> 8U);
}

}  // namespace

std::vector<std::string> AsciiArtRenderer::render(int cols, int rows, std::string_view alt,
                                                   std::string_view src) const {
    if (cols <= 0 || rows <= 0) {
        return {};
    }

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

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(rows));

    for (int cr = 0; cr < rows; ++cr) {
        std::string line;
        line.reserve(static_cast<size_t>(cols));
        for (int cc = 0; cc < cols; ++cc) {
            // Map this cell to a source pixel block, average its luminance.
            const int sx0 = img_w * cc / cols;
            const int sx1 = std::max(sx0 + 1, img_w * (cc + 1) / cols);
            const int sy0 = img_h * cr / rows;
            const int sy1 = std::max(sy0 + 1, img_h * (cr + 1) / rows);

            unsigned long sum = 0;
            int count = 0;
            for (int sy = sy0; sy < std::min(sy1, img_h); ++sy) {
                for (int sx = sx0; sx < std::min(sx1, img_w); ++sx) {
                    const size_t idx = (static_cast<size_t>(sy) * img_w + sx) * 4;
                    sum += luminance(img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2]);
                    ++count;
                }
            }
            const uint8_t avg = count > 0 ? static_cast<uint8_t>(sum / static_cast<unsigned>(count)) : 0;
            const size_t level = std::min<size_t>(k_ramp.size() - 1,
                                                   static_cast<size_t>(avg) * k_ramp.size() / 256);
            line += k_ramp[level];
        }
        result.push_back(std::move(line));
    }
    return result;
}

}  // namespace tvshow::images
