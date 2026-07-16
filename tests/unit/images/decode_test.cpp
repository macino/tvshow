#include "tvshow/images/decode.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using tvshow::images::decode_image;

namespace {

// Builds a minimal uncompressed 24bpp BMP file for a WxH image where every
// pixel has color {r,g,b}. BMP is the simplest format stb_image supports,
// so it's the cheapest fixture to hand-construct byte-for-byte.
std::string make_bmp(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    const int row_bytes = w * 3;
    const int row_padded = (row_bytes + 3) & ~3;
    const int pixel_data_size = row_padded * h;
    const int file_size = 14 + 40 + pixel_data_size;

    std::vector<uint8_t> buf;
    buf.reserve(static_cast<size_t>(file_size));

    auto put_u16 = [&](uint16_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto put_u32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };

    // BITMAPFILEHEADER
    buf.push_back('B');
    buf.push_back('M');
    put_u32(static_cast<uint32_t>(file_size));
    put_u32(0);   // reserved
    put_u32(54);  // pixel data offset (14 + 40)

    // BITMAPINFOHEADER
    put_u32(40);                     // header size
    put_u32(static_cast<uint32_t>(w));
    put_u32(static_cast<uint32_t>(h));
    put_u16(1);                      // planes
    put_u16(24);                     // bpp
    put_u32(0);                      // compression: none
    put_u32(static_cast<uint32_t>(pixel_data_size));
    put_u32(0);
    put_u32(0);
    put_u32(0);
    put_u32(0);

    // Pixel data: bottom-up rows, BGR order, padded to 4 bytes.
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            buf.push_back(b);
            buf.push_back(g);
            buf.push_back(r);
        }
        for (int p = row_bytes; p < row_padded; ++p) {
            buf.push_back(0);
        }
    }

    return {buf.begin(), buf.end()};
}

}  // namespace

TEST_CASE("decode_image: decodes a solid-color BMP into RGBA pixel data") {
    const std::string bmp = make_bmp(2, 2, 0xAA, 0xBB, 0xCC);
    const auto img = decode_image(bmp);
    REQUIRE(img.has_value());
    CHECK(img->width == 2);
    CHECK(img->height == 2);
    REQUIRE(img->pixels.size() == static_cast<size_t>(2 * 2 * 4));
    CHECK(img->pixels[0] == 0xAA);
    CHECK(img->pixels[1] == 0xBB);
    CHECK(img->pixels[2] == 0xCC);
    CHECK(img->pixels[3] == 0xFF);  // alpha forced opaque for RGB source
}

TEST_CASE("decode_image: returns nullopt for empty input") {
    CHECK_FALSE(decode_image("").has_value());
}

TEST_CASE("decode_image: returns nullopt for garbage/corrupt bytes") {
    CHECK_FALSE(decode_image("not an image").has_value());
}
