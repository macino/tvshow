#include "tvshow/render/chargrid.hpp"

#include "tvshow/types.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace tvshow::render {

namespace {

void utf8_encode(char32_t cp, std::string& out) {
    if (cp < 0x80U) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800U) {
        out += static_cast<char>(static_cast<uint8_t>(0xC0U | (cp >> 6U)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | (cp & 0x3FU)));
    } else if (cp < 0x10000U) {
        out += static_cast<char>(static_cast<uint8_t>(0xE0U | (cp >> 12U)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | ((cp >> 6U) & 0x3FU)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | (cp & 0x3FU)));
    } else {
        out += static_cast<char>(static_cast<uint8_t>(0xF0U | (cp >> 18U)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | ((cp >> 12U) & 0x3FU)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | ((cp >> 6U) & 0x3FU)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | (cp & 0x3FU)));
    }
}

// Decode one UTF-8 code point starting at s[i]; advance i past the bytes consumed.
// Returns U+FFFD on any invalid sequence.
char32_t utf8_decode(std::string_view s, size_t& i) {
    const auto lead = static_cast<uint8_t>(s.at(i));
    ++i;

    if (lead < 0x80U)
        return static_cast<char32_t>(lead);

    auto cont = [&](uint8_t& b) -> bool {
        if (i >= s.size())
            return false;
        b = static_cast<uint8_t>(s.at(i++));
        return (b & 0xC0U) == 0x80U;
    };

    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;

    if ((lead & 0xE0U) == 0xC0U) {
        if (!cont(b1))
            return U'⁻';
        return static_cast<char32_t>(((lead & 0x1FU) << 6U) | (b1 & 0x3FU));
    }
    if ((lead & 0xF0U) == 0xE0U) {
        if (!cont(b1) || !cont(b2))
            return U'⁻';
        return static_cast<char32_t>(((lead & 0x0FU) << 12U) | ((b1 & 0x3FU) << 6U) | (b2 & 0x3FU));
    }
    if ((lead & 0xF8U) == 0xF0U) {
        if (!cont(b1) || !cont(b2) || !cont(b3))
            return U'⁻';
        return static_cast<char32_t>(((lead & 0x07U) << 18U) | ((b1 & 0x3FU) << 12U) |
                                     ((b2 & 0x3FU) << 6U) | (b3 & 0x3FU));
    }
    return U'⁻';
}

std::string encode_flags(const ColorAttr& a) {
    std::string f;
    if (a.bold)
        f += 'b';
    if (a.italic)
        f += 'i';
    if (a.underline)
        f += 'u';
    if (a.strike)
        f += 's';
    if (f.empty())
        f = "-";
    return f;
}

std::optional<uint32_t> parse_hex6(std::string_view s) {
    if (s.size() < 6)
        return std::nullopt;
    uint32_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + 6, v, 16);
    if (ec != std::errc{} || ptr != s.data() + 6)
        return std::nullopt;
    return v;
}

// Parse header line "COLS=N ROWS=M\n", advance s, fill cols/rows.
bool parse_grid_header(std::string_view& s, int& cols, int& rows) {
    const auto nl = s.find('\n');
    if (nl == std::string_view::npos)
        return false;

    auto hdr = s.substr(0, nl);
    s.remove_prefix(nl + 1);

    if (!hdr.starts_with("COLS="))
        return false;
    hdr.remove_prefix(5);

    const auto sp = hdr.find(' ');
    if (sp == std::string_view::npos)
        return false;

    {
        auto [ptr, ec] = std::from_chars(hdr.data(), hdr.data() + sp, cols);
        if (ec != std::errc{} || ptr != hdr.data() + sp)
            return false;
    }
    hdr.remove_prefix(sp + 1);

    if (!hdr.starts_with("ROWS="))
        return false;
    hdr.remove_prefix(5);

    auto [ptr, ec] = std::from_chars(hdr.data(), hdr.data() + hdr.size(), rows);
    return ec == std::errc{} && cols > 0 && rows > 0;
}

// Parse one character row from s into cells at row_idx.
bool parse_char_row(std::string_view& s, std::vector<Cell>& cells, int cols, int row_idx) {
    const auto eol = s.find('\n');
    if (eol == std::string_view::npos)
        return false;
    auto row = s.substr(0, eol);
    s.remove_prefix(eol + 1);

    size_t byte_i = 0;
    const size_t base = static_cast<size_t>(row_idx) * static_cast<size_t>(cols);
    for (int c = 0; c < cols; ++c) {
        const char32_t cp = byte_i < row.size() ? utf8_decode(row, byte_i) : U' ';
        cells.at(base + static_cast<size_t>(c)).cp = cp;
    }
    return true;
}

// Parse one "rrggbb:rrggbb:flags" cell from the front of row; advance row.
bool parse_cell_attr(std::string_view& row, ColorAttr& attr) {
    const auto fg = parse_hex6(row);
    if (!fg)
        return false;
    row.remove_prefix(6);

    if (row.empty() || row.front() != ':')
        return false;
    row.remove_prefix(1);

    const auto bg = parse_hex6(row);
    if (!bg)
        return false;
    row.remove_prefix(6);

    if (row.empty() || row.front() != ':')
        return false;
    row.remove_prefix(1);

    const auto flag_end = row.find(' ');
    const auto flags = flag_end == std::string_view::npos ? row : row.substr(0, flag_end);
    row.remove_prefix(flags.size());

    attr.fg = *fg;
    attr.bg = *bg;
    for (const char f : flags) {
        switch (f) {
        case 'b':
            attr.bold = true;
            break;
        case 'i':
            attr.italic = true;
            break;
        case 'u':
            attr.underline = true;
            break;
        case 's':
            attr.strike = true;
            break;
        case '-':
            break;
        default:
            return false;
        }
    }
    return true;
}

// Parse one attribute row from s into cells at row_idx.
bool parse_attr_row(std::string_view& s, std::vector<Cell>& cells, int cols, int row_idx) {
    const auto eol = s.find('\n');
    if (eol == std::string_view::npos)
        return false;
    auto row = s.substr(0, eol);
    s.remove_prefix(eol + 1);

    const size_t base = static_cast<size_t>(row_idx) * static_cast<size_t>(cols);
    for (int c = 0; c < cols; ++c) {
        if (c > 0) {
            if (row.empty() || row.front() != ' ')
                return false;
            row.remove_prefix(1);
        }
        if (!parse_cell_attr(row, cells.at(base + static_cast<size_t>(c)).attr))
            return false;
    }
    return true;
}

}  // namespace

CharGrid::CharGrid(int cols, int rows)
    : cols_(cols), rows_(rows), cells_(static_cast<size_t>(cols) * static_cast<size_t>(rows)) {}

void CharGrid::put(Point pos, char32_t cp, const ColorAttr& attr) {
    if (pos.col < 0 || pos.col >= cols_ || pos.row < 0 || pos.row >= rows_)
        throw std::out_of_range("CharGrid::put: position out of range");
    cells_.at(index(pos)) = Cell{cp, attr};
}

const Cell& CharGrid::at(Point pos) const {
    if (pos.col < 0 || pos.col >= cols_ || pos.row < 0 || pos.row >= rows_)
        throw std::out_of_range("CharGrid::at: position out of range");
    return cells_.at(index(pos));
}

std::string CharGrid::to_string() const {
    std::string s;
    s.reserve(static_cast<size_t>(cols_ * rows_) * 20U);
    s += std::format("COLS={} ROWS={}\n", cols_, rows_);

    size_t idx = 0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c, ++idx) {
            char32_t cp = cells_.at(idx).cp;
            if (cp < 0x20U || cp == 0x7FU)
                cp = U' ';
            utf8_encode(cp, s);
        }
        s += '\n';
    }

    s += "===\n";

    idx = 0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c, ++idx) {
            if (c > 0)
                s += ' ';
            const ColorAttr& a = cells_.at(idx).attr;
            s += std::format("{:06x}:{:06x}:{}", a.fg, a.bg, encode_flags(a));
        }
        s += '\n';
    }

    return s;
}

std::optional<CharGrid> CharGrid::from_string(std::string_view s) {
    int cols = 0;
    int rows = 0;
    if (!parse_grid_header(s, cols, rows))
        return std::nullopt;

    CharGrid grid(cols, rows);

    for (int r = 0; r < rows; ++r) {
        if (!parse_char_row(s, grid.cells_, cols, r))
            return std::nullopt;
    }

    if (!s.starts_with("===\n"))
        return std::nullopt;
    s.remove_prefix(4);

    for (int r = 0; r < rows; ++r) {
        if (!parse_attr_row(s, grid.cells_, cols, r))
            return std::nullopt;
    }

    return grid;
}

}  // namespace tvshow::render
