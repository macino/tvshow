#pragma once

namespace tvshow {

struct Point {
    int col = 0;
    int row = 0;
    [[nodiscard]] bool operator==(const Point&) const noexcept = default;
};

struct Size {
    int cols = 0;
    int rows = 0;
    [[nodiscard]] bool operator==(const Size&) const noexcept = default;
};

struct Rect {
    Point origin;
    Size size;

    [[nodiscard]] bool empty() const noexcept { return size.cols <= 0 || size.rows <= 0; }

    [[nodiscard]] bool contains(Point p) const noexcept {
        return p.col >= origin.col && p.col < origin.col + size.cols && p.row >= origin.row &&
               p.row < origin.row + size.rows;
    }

    [[nodiscard]] bool operator==(const Rect&) const noexcept = default;
};

}  // namespace tvshow
