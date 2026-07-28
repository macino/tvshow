#include "tvshow/images/renderer.hpp"

#include <doctest/doctest.h>

using tvshow::images::AsciiArtRenderer;
using tvshow::images::ImageCache;
using tvshow::images::ImageData;

TEST_CASE("AsciiArtRenderer: falls back to alt text when cache is null") {
    AsciiArtRenderer renderer(nullptr);
    const auto lines = renderer.render(5, 2, "pic", "x.png");
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find("pic") != std::string::npos);
}

TEST_CASE("AsciiArtRenderer: falls back to alt text on cache miss") {
    ImageCache cache;
    AsciiArtRenderer renderer(&cache);
    const auto lines = renderer.render(5, 2, "pic", "missing.png");
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find("pic") != std::string::npos);
}

TEST_CASE("AsciiArtRenderer: returns exactly rows x cols for a cached image") {
    ImageCache cache;
    ImageData img;
    img.width = 4;
    img.height = 4;
    img.pixels.assign(4 * 4 * 4, 128);  // mid-gray RGBA
    cache["pic.png"] = img;

    AsciiArtRenderer renderer(&cache);
    const auto lines = renderer.render(3, 2, "alt", "pic.png");
    REQUIRE(lines.size() == 2);
    for (const auto& line : lines) {
        CHECK(line.size() == 3);
    }
}

TEST_CASE("AsciiArtRenderer: black pixels render the darkest ramp char") {
    ImageCache cache;
    ImageData img;
    img.width = 1;
    img.height = 1;
    img.pixels = {0, 0, 0, 255};  // black
    cache["black.png"] = img;

    AsciiArtRenderer renderer(&cache);
    const auto lines = renderer.render(1, 1, "alt", "black.png");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0][0] == ' ');  // darkest ramp level
}

TEST_CASE("AsciiArtRenderer: white pixels render the lightest ramp char") {
    ImageCache cache;
    ImageData img;
    img.width = 1;
    img.height = 1;
    img.pixels = {255, 255, 255, 255};  // white
    cache["white.png"] = img;

    AsciiArtRenderer renderer(&cache);
    const auto lines = renderer.render(1, 1, "alt", "white.png");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0][0] == '@');  // lightest ramp level
}

TEST_CASE("AsciiArtRenderer: cols<=0 or rows<=0 yields empty result") {
    ImageCache cache;
    AsciiArtRenderer renderer(&cache);
    CHECK(renderer.render(0, 5, "a", "x").empty());
    CHECK(renderer.render(5, 0, "a", "x").empty());
}
