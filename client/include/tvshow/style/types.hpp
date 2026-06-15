#pragma once

#include <array>
#include <cstdint>

namespace tvshow::style {

// ── Length ──────────────────────────────────────────────────────────────────

enum class LengthUnit : uint8_t { Px, Pct, Em, Ch, Rem };

struct Length {
    float value = 0;
    LengthUnit unit = LengthUnit::Px;
    bool is_auto = true;  // "auto" or unset

    [[nodiscard]] bool operator==(const Length&) const noexcept = default;
};

inline Length px(float v) noexcept {
    return {v, LengthUnit::Px, false};
}
inline Length pct(float v) noexcept {
    return {v, LengthUnit::Pct, false};
}
inline Length ch(float v) noexcept {
    return {v, LengthUnit::Ch, false};
}
inline Length em(float v) noexcept {
    return {v, LengthUnit::Em, false};
}
inline Length rem(float v) noexcept {
    return {v, LengthUnit::Rem, false};
}
inline Length len_auto() noexcept {
    return {};
}  // is_auto = true

struct Edges {
    Length top{}, right{}, bottom{}, left{};
    [[nodiscard]] bool operator==(const Edges&) const noexcept = default;
};

// ── Color ────────────────────────────────────────────────────────────────────

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    bool none = true;  // transparent / unset

    [[nodiscard]] bool operator==(const Color&) const noexcept = default;
};

inline Color rgb(uint8_t r, uint8_t g, uint8_t b) noexcept {
    return {r, g, b, false};
}
inline Color color_none() noexcept {
    return {};
}  // transparent

// ── Enumerations ─────────────────────────────────────────────────────────────

enum class Display : uint8_t { Block, Inline, InlineBlock, Flex, None };
enum class FontWeight : uint8_t { Normal, Bold };
enum class FontStyle : uint8_t { Normal, Italic };
enum class TextDecoration : uint8_t { None, Underline, LineThrough };
enum class TextAlign : uint8_t { Left, Center, Right };
enum class WhiteSpace : uint8_t { Normal, Pre, Nowrap };
enum class BorderStyle : uint8_t {
    None,
    Solid,
    Double,
    Dashed,
    Dotted,
    Ridge,
    Groove,
    Inset,
    Outset
};
enum class FlexDirection : uint8_t { Row, RowReverse, Column, ColumnReverse };
enum class JustifyContent : uint8_t { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround };
enum class AlignItems : uint8_t { FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class Visibility : uint8_t { Visible, Hidden };
enum class Overflow : uint8_t { Visible, Hidden, Scroll, Auto };

// ── Border ───────────────────────────────────────────────────────────────────

struct BorderSide {
    Length width{};
    BorderStyle style = BorderStyle::None;
    Color color{};
    [[nodiscard]] bool operator==(const BorderSide&) const noexcept = default;
};

// top=0, right=1, bottom=2, left=3
using BorderBox = std::array<BorderSide, 4>;

// ── ComputedStyle ─────────────────────────────────────────────────────────────

struct ComputedStyle {
    Display display = Display::Inline;
    FontWeight font_weight = FontWeight::Normal;
    FontStyle font_style = FontStyle::Normal;
    TextDecoration text_decoration = TextDecoration::None;
    TextAlign text_align = TextAlign::Left;
    WhiteSpace white_space = WhiteSpace::Normal;
    Color color{};
    Color background{};
    Length width{};
    Length height{};
    Length min_width{};
    Length max_width{};
    Edges margin{};
    Edges padding{};
    BorderBox border{};
    FlexDirection flex_direction = FlexDirection::Row;
    JustifyContent justify_content = JustifyContent::FlexStart;
    AlignItems align_items = AlignItems::Stretch;
    Length gap{};
    float flex_grow = 0.0F;
    float flex_shrink = 1.0F;
    Length flex_basis{};
    Visibility visibility = Visibility::Visible;
    Overflow overflow = Overflow::Visible;
};

}  // namespace tvshow::style
