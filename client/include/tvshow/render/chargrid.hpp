#pragma once

#include "tvshow/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::render {

struct ColorAttr {
    uint32_t fg = 0xFFFFFFU;
    uint32_t bg = 0x000000U;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;

    [[nodiscard]] bool operator==(const ColorAttr&) const noexcept = default;
};

struct Cell {
    char32_t cp = U' ';
    ColorAttr attr;

    [[nodiscard]] bool operator==(const Cell&) const noexcept = default;
};

// 2-D grid of character cells — the pure output of the render stage.
// Serializable for golden-test snapshots.
class CharGrid {
public:
    CharGrid(int cols, int rows);

    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] int rows() const noexcept { return rows_; }

    // Write one cell. Throws std::out_of_range if pos is outside the grid.
    void put(Point pos, char32_t cp, const ColorAttr& attr);

    // Read one cell. Throws std::out_of_range if pos is outside the grid.
    [[nodiscard]] const Cell& at(Point pos) const;

    // Serialize to the golden-snapshot format (UTF-8 text).
    [[nodiscard]] std::string to_string() const;

    // Deserialize from a golden-snapshot string. Returns nullopt on parse error.
    [[nodiscard]] static std::optional<CharGrid> from_string(std::string_view s);

private:
    int cols_;
    int rows_;
    std::vector<Cell> cells_;

    [[nodiscard]] size_t index(Point pos) const noexcept {
        return static_cast<size_t>(pos.row) * static_cast<size_t>(cols_) +
               static_cast<size_t>(pos.col);
    }
};

}  // namespace tvshow::render
