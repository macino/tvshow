#include "tvshow/style/resolver.hpp"

#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace tvshow::style {

namespace {

// ── Color helpers ─────────────────────────────────────────────────────────────

struct NamedColor {
    std::string_view name;
    uint8_t r, g, b;
};

constexpr std::array<NamedColor, 17> k_named_colors{{
    {"black", 0, 0, 0},
    {"white", 255, 255, 255},
    {"red", 255, 0, 0},
    {"green", 0, 128, 0},
    {"blue", 0, 0, 255},
    {"yellow", 255, 255, 0},
    {"cyan", 0, 255, 255},
    {"magenta", 255, 0, 255},
    {"gray", 128, 128, 128},
    {"grey", 128, 128, 128},
    {"orange", 255, 165, 0},
    {"pink", 255, 192, 203},
    {"purple", 128, 0, 128},
    {"brown", 165, 42, 42},
    {"navy", 0, 0, 128},
    {"teal", 0, 128, 128},
    {"silver", 192, 192, 192},
}};

uint8_t hex2(char hi, char lo) noexcept {
    auto hex = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') {
            return static_cast<uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<uint8_t>(10 + c - 'a');
        }
        return static_cast<uint8_t>(10 + c - 'A');
    };
    return static_cast<uint8_t>(hex(hi) * 16 + hex(lo));
}

const char* skip_ws(const char* p, const char* end) noexcept {
    while (p < end && std::isspace(static_cast<unsigned char>(*p)) != 0) {
        ++p;
    }
    return p;
}

Color parse_rgb_func(std::string_view inner) noexcept {
    unsigned ur = 0;
    unsigned ug = 0;
    unsigned ub = 0;
    const char* p = inner.data();
    const char* end = p + inner.size();
    p = skip_ws(p, end);
    const auto [p1, ec1] = std::from_chars(p, end, ur);
    if (ec1 != std::errc{}) {
        return color_none();
    }
    p = skip_ws(p1, end);
    if (p >= end || *p != ',') {
        return color_none();
    }
    p = skip_ws(p + 1, end);
    const auto [p2, ec2] = std::from_chars(p, end, ug);
    if (ec2 != std::errc{}) {
        return color_none();
    }
    p = skip_ws(p2, end);
    if (p >= end || *p != ',') {
        return color_none();
    }
    p = skip_ws(p + 1, end);
    const auto [p3, ec3] = std::from_chars(p, end, ub);
    if (ec3 != std::errc{}) {
        return color_none();
    }
    const auto r = static_cast<uint8_t>(ur > 255U ? 255U : ur);
    const auto g = static_cast<uint8_t>(ug > 255U ? 255U : ug);
    const auto b = static_cast<uint8_t>(ub > 255U ? 255U : ub);
    return rgb(r, g, b);
}

}  // namespace

Color parse_color(std::string_view s) noexcept {
    if (s.empty() || s == "transparent" || s == "none") {
        return color_none();
    }
    if (s.starts_with('#')) {
        if (s.size() == 4) {
            return rgb(hex2(s[1], s[1]), hex2(s[2], s[2]), hex2(s[3], s[3]));
        }
        if (s.size() == 7) {
            return rgb(hex2(s[1], s[2]), hex2(s[3], s[4]), hex2(s[5], s[6]));
        }
        return color_none();
    }
    if (s.starts_with("rgb(") && s.ends_with(')')) {
        return parse_rgb_func(s.substr(4, s.size() - 5));
    }
    for (const auto& nc : k_named_colors) {
        if (s == nc.name) {
            return rgb(nc.r, nc.g, nc.b);
        }
    }
    return color_none();
}

// ── Length parsing ────────────────────────────────────────────────────────────

Length parse_length(std::string_view s) noexcept {
    if (s.empty() || s == "auto" || s == "none") {
        return len_auto();
    }
    if (s == "0") {
        return px(0);
    }
    float val = 0;
    const char* begin = s.data();
    const char* end = begin + s.size();
    const auto [num_end, ec] = std::from_chars(begin, end, val);
    if (ec != std::errc{}) {
        return len_auto();
    }
    const std::string_view unit(num_end, static_cast<std::size_t>(end - num_end));
    if (unit == "px") {
        return px(val);
    }
    if (unit == "%") {
        return pct(val);
    }
    if (unit == "ch") {
        return ch(val);
    }
    if (unit == "em") {
        return em(val);
    }
    if (unit == "rem") {
        return rem(val);
    }
    if (unit.empty()) {
        return px(val);
    }
    return len_auto();
}

// ── UA stylesheet ─────────────────────────────────────────────────────────────

namespace {

constexpr std::string_view k_ua_css = R"css(
head { display: none; }
body { display: block; }
center { display: block; text-align: center; }
div { display: block; }
p { display: block; margin-top: 0; margin-bottom: 16px; }
section { display: block; }
article { display: block; }
header { display: block; }
footer { display: block; }
nav { display: block; }
main { display: block; }
aside { display: block; }
h1 { display: block; font-weight: bold; margin-top: 16px; margin-bottom: 8px; }
h2 { display: block; font-weight: bold; margin-top: 16px; margin-bottom: 8px; }
h3 { display: block; font-weight: bold; margin-top: 8px; margin-bottom: 0; }
h4 { display: block; font-weight: bold; margin-top: 8px; margin-bottom: 0; }
h5 { display: block; font-weight: bold; margin-top: 8px; margin-bottom: 0; }
h6 { display: block; font-weight: bold; margin-top: 8px; margin-bottom: 0; }
ul { display: block; margin-top: 0; margin-bottom: 16px; padding-left: 16px; }
ol { display: block; margin-top: 0; margin-bottom: 16px; padding-left: 16px; }
li { display: block; }
blockquote { display: block; margin-top: 16px; margin-bottom: 16px; margin-left: 16px; }
hr { display: block; margin-top: 8px; margin-bottom: 8px; }
span { display: inline; }
small { display: inline; }
u { display: inline; text-decoration: underline; }
a { display: inline; color: #5555ff; text-decoration: underline; }
b { display: inline; font-weight: bold; }
strong { display: inline; font-weight: bold; }
i { display: inline; font-style: italic; }
em { display: inline; font-style: italic; }
pre { display: block; white-space: pre; margin-top: 0; margin-bottom: 16px; }
code { display: inline; white-space: pre; }
label { display: inline; }
form { display: block; }
input { display: inline-block; }
button { display: inline-block; }
textarea { display: block; }
select { display: inline-block; }
option { display: none; }
optgroup { display: none; }
img { display: inline-block; }
script { display: none; }
noscript { display: none; }
style { display: none; }
template { display: none; }
table { display: block; }
caption { display: block; }
thead { display: block; }
tbody { display: block; }
tfoot { display: block; }
tr { display: block; }
td { display: inline; }
th { display: inline; font-weight: bold; }
colgroup { display: none; }
col { display: none; }
)css";

}  // namespace

const css::Stylesheet& ua_stylesheet() {
    static const css::Stylesheet sheet = [] {
        auto result = css::parse(k_ua_css);
        return result.value_or(css::Stylesheet{});
    }();
    return sheet;
}

// ── Selector matching ─────────────────────────────────────────────────────────

namespace {

bool match_simple(const css::SimpleSel& s, const dom::Node& node) {
    if (node.kind != dom::NodeKind::Element) {
        return false;
    }
    switch (s.kind) {
    case css::SimpleSel::Kind::Universal:
        return true;
    case css::SimpleSel::Kind::Tag:
        return node.tag == s.value;
    case css::SimpleSel::Kind::Class:
        return node.has_class(s.value);
    case css::SimpleSel::Kind::Id:
        return node.attr("id") == s.value;
    case css::SimpleSel::Kind::AttrExists:
        return !node.attr(s.value).empty();
    case css::SimpleSel::Kind::PseudoClass:
        return false;  // :hover/:focus not matchable statically
    }
    return false;
}

bool match_compound(const css::CompoundSel& c, const dom::Node& node) {
    return std::all_of(c.simples.begin(), c.simples.end(),
                       [&](const css::SimpleSel& s) { return match_simple(s, node); });
}

// ancestors: from root down to immediate parent (oldest first).
// NOLINTNEXTLINE(misc-no-recursion)
bool match_complex(const css::ComplexSel& sel, const dom::Node& node,
                   std::span<const dom::Node* const> ancestors) {
    const std::size_t n = sel.compounds.size();
    if (n == 0) {
        return false;
    }
    if (!match_compound(sel.compounds[n - 1], node)) {
        return false;
    }
    if (n == 1) {
        return true;
    }
    std::size_t anc_end = ancestors.size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const css::Combinator comb = sel.combinators[n - 2 - i];
        const css::CompoundSel& needed = sel.compounds[n - 2 - i];
        if (comb == css::Combinator::Child) {
            if (anc_end == 0) {
                return false;
            }
            const dom::Node* parent = ancestors[anc_end - 1];
            if (parent == nullptr || !match_compound(needed, *parent)) {
                return false;
            }
            --anc_end;
        } else {
            bool found = false;
            for (std::size_t j = anc_end; j > 0; --j) {
                const dom::Node* anc = ancestors[j - 1];
                if (anc != nullptr && match_compound(needed, *anc)) {
                    anc_end = j - 1;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
    }
    return true;
}

// ── Cascade ───────────────────────────────────────────────────────────────────

struct MatchedDecl {
    int specificity = 0;
    std::size_t order = 0;
    const css::Declaration* decl = nullptr;
    bool important = false;
};

void collect_from_sheet(const dom::Node& node, std::span<const dom::Node* const> ancestors,
                        const css::Stylesheet& sheet, bool is_ua, std::size_t& order_counter,
                        std::vector<MatchedDecl>& out) {
    for (const auto& rule : sheet.rules) {
        for (const auto& sel : rule.selectors) {
            if (!match_complex(sel, node, ancestors)) {
                continue;
            }
            const int spec = is_ua ? 0 : sel.specificity;
            for (const auto& decl : rule.declarations) {
                out.push_back({spec, order_counter++, &decl, decl.important});
            }
            break;  // match once per rule
        }
    }
}

// ── Property application ──────────────────────────────────────────────────────

void apply_edges(Edges& edges, std::string_view val) {
    const Length l = parse_length(val);
    edges.top = l;
    edges.right = l;
    edges.bottom = l;
    edges.left = l;
}

BorderStyle parse_border_style_val(std::string_view val) noexcept {
    if (val == "solid") {
        return BorderStyle::Solid;
    }
    if (val == "double") {
        return BorderStyle::Double;
    }
    if (val == "dashed") {
        return BorderStyle::Dashed;
    }
    if (val == "dotted") {
        return BorderStyle::Dotted;
    }
    if (val == "ridge") {
        return BorderStyle::Ridge;
    }
    if (val == "groove") {
        return BorderStyle::Groove;
    }
    if (val == "inset") {
        return BorderStyle::Inset;
    }
    if (val == "outset") {
        return BorderStyle::Outset;
    }
    return BorderStyle::None;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool apply_color_props(ComputedStyle& style, std::string_view prop, std::string_view val) {
    if (prop == "color") {
        style.color = parse_color(val);
    } else if (prop == "background-color" || prop == "background") {
        style.background = parse_color(val);
    } else {
        return false;
    }
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool apply_text_props(ComputedStyle& style, std::string_view prop, std::string_view val) {
    if (prop == "font-weight") {
        style.font_weight = (val == "bold") ? FontWeight::Bold : FontWeight::Normal;
    } else if (prop == "font-style") {
        style.font_style = (val == "italic") ? FontStyle::Italic : FontStyle::Normal;
    } else if (prop == "text-decoration") {
        if (val == "underline") {
            style.text_decoration = TextDecoration::Underline;
        } else if (val == "line-through") {
            style.text_decoration = TextDecoration::LineThrough;
        } else {
            style.text_decoration = TextDecoration::None;
        }
    } else if (prop == "text-align") {
        if (val == "center") {
            style.text_align = TextAlign::Center;
        } else if (val == "right") {
            style.text_align = TextAlign::Right;
        } else {
            style.text_align = TextAlign::Left;
        }
    } else if (prop == "white-space") {
        if (val == "pre") {
            style.white_space = WhiteSpace::Pre;
        } else if (val == "nowrap") {
            style.white_space = WhiteSpace::Nowrap;
        } else {
            style.white_space = WhiteSpace::Normal;
        }
    } else {
        return false;
    }
    return true;
}

Display parse_display_val(std::string_view val) noexcept {
    if (val == "block") {
        return Display::Block;
    }
    if (val == "inline-block") {
        return Display::InlineBlock;
    }
    if (val == "flex") {
        return Display::Flex;
    }
    if (val == "none") {
        return Display::None;
    }
    return Display::Inline;
}

FlexDirection parse_flex_direction_val(std::string_view val) noexcept {
    if (val == "column") {
        return FlexDirection::Column;
    }
    if (val == "column-reverse") {
        return FlexDirection::ColumnReverse;
    }
    if (val == "row-reverse") {
        return FlexDirection::RowReverse;
    }
    return FlexDirection::Row;
}

JustifyContent parse_justify_content_val(std::string_view val) noexcept {
    if (val == "center") {
        return JustifyContent::Center;
    }
    if (val == "flex-end" || val == "end") {
        return JustifyContent::FlexEnd;
    }
    if (val == "space-between") {
        return JustifyContent::SpaceBetween;
    }
    if (val == "space-around") {
        return JustifyContent::SpaceAround;
    }
    return JustifyContent::FlexStart;
}

AlignItems parse_align_items_val(std::string_view val) noexcept {
    if (val == "center") {
        return AlignItems::Center;
    }
    if (val == "flex-end" || val == "end") {
        return AlignItems::FlexEnd;
    }
    if (val == "flex-start" || val == "start") {
        return AlignItems::FlexStart;
    }
    if (val == "baseline") {
        return AlignItems::Baseline;
    }
    return AlignItems::Stretch;
}

// Parse the border shorthand: [<width>] [<style>] [<color>]
// Tokens are space-separated; order doesn't matter.
void apply_border_shorthand(ComputedStyle& style, std::string_view val) {
    size_t i = 0;
    while (i < val.size()) {
        while (i < val.size() && val[i] == ' ') {
            ++i;
        }
        const size_t j = val.find(' ', i);
        const std::string_view tok =
            (j == std::string_view::npos) ? val.substr(i) : val.substr(i, j - i);
        if (tok.empty()) {
            break;
        }
        const BorderStyle bs = parse_border_style_val(tok);
        if (bs != BorderStyle::None || tok == "none") {
            for (auto& side : style.border) {
                side.style = bs;
            }
        } else {
            const Length w = parse_length(tok);
            if (!w.is_auto) {
                for (auto& side : style.border) {
                    side.width = w;
                }
            } else {
                const Color c = parse_color(tok);
                if (!c.none) {
                    for (auto& side : style.border) {
                        side.color = c;
                    }
                }
            }
        }
        i = (j == std::string_view::npos) ? val.size() : j + 1;
    }
}

// Apply one side of the border shorthand (border-top, border-right, etc.).
void apply_border_side(BorderSide& side, std::string_view val) {
    size_t i = 0;
    while (i < val.size()) {
        while (i < val.size() && val[i] == ' ') {
            ++i;
        }
        const size_t j = val.find(' ', i);
        const std::string_view tok =
            (j == std::string_view::npos) ? val.substr(i) : val.substr(i, j - i);
        if (tok.empty()) {
            break;
        }
        const BorderStyle bs = parse_border_style_val(tok);
        if (bs != BorderStyle::None || tok == "none") {
            side.style = bs;
        } else {
            const Length w = parse_length(tok);
            if (!w.is_auto) {
                side.width = w;
            } else {
                const Color c = parse_color(tok);
                if (!c.none) {
                    side.color = c;
                }
            }
        }
        i = (j == std::string_view::npos) ? val.size() : j + 1;
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool apply_box_props(ComputedStyle& style, std::string_view prop, std::string_view val) {
    if (prop == "display") {
        style.display = parse_display_val(val);
    } else if (prop == "width") {
        style.width = parse_length(val);
    } else if (prop == "height") {
        style.height = parse_length(val);
    } else if (prop == "min-width") {
        style.min_width = parse_length(val);
    } else if (prop == "max-width") {
        style.max_width = parse_length(val);
    } else if (prop == "margin") {
        apply_edges(style.margin, val);
    } else if (prop == "margin-top") {
        style.margin.top = parse_length(val);
    } else if (prop == "margin-right") {
        style.margin.right = parse_length(val);
    } else if (prop == "margin-bottom") {
        style.margin.bottom = parse_length(val);
    } else if (prop == "margin-left") {
        style.margin.left = parse_length(val);
    } else if (prop == "padding") {
        apply_edges(style.padding, val);
    } else if (prop == "padding-top") {
        style.padding.top = parse_length(val);
    } else if (prop == "padding-right") {
        style.padding.right = parse_length(val);
    } else if (prop == "padding-bottom") {
        style.padding.bottom = parse_length(val);
    } else if (prop == "padding-left") {
        style.padding.left = parse_length(val);
    } else if (prop == "border") {
        apply_border_shorthand(style, val);
    } else if (prop == "border-top") {
        apply_border_side(style.border[0], val);
    } else if (prop == "border-right") {
        apply_border_side(style.border[1], val);
    } else if (prop == "border-bottom") {
        apply_border_side(style.border[2], val);
    } else if (prop == "border-left") {
        apply_border_side(style.border[3], val);
    } else if (prop == "border-style") {
        const BorderStyle bs = parse_border_style_val(val);
        for (auto& side : style.border) {
            side.style = bs;
        }
    } else if (prop == "border-width") {
        const Length bw = parse_length(val);
        for (auto& side : style.border) {
            side.width = bw;
        }
    } else if (prop == "border-color") {
        const Color bc = parse_color(val);
        for (auto& side : style.border) {
            side.color = bc;
        }
    } else {
        return false;
    }
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool apply_flex_props(ComputedStyle& style, std::string_view prop, std::string_view val) {
    if (prop == "flex-direction") {
        style.flex_direction = parse_flex_direction_val(val);
    } else if (prop == "justify-content") {
        style.justify_content = parse_justify_content_val(val);
    } else if (prop == "align-items") {
        style.align_items = parse_align_items_val(val);
    } else if (prop == "gap") {
        style.gap = parse_length(val);
    } else if (prop == "flex-grow") {
        float v = 0;
        std::from_chars(val.data(), val.data() + val.size(), v);
        style.flex_grow = v;
    } else if (prop == "flex-shrink") {
        float v = 1;
        std::from_chars(val.data(), val.data() + val.size(), v);
        style.flex_shrink = v;
    } else if (prop == "flex-basis") {
        style.flex_basis = parse_length(val);
    } else {
        return false;
    }
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool apply_other_props(ComputedStyle& style, std::string_view prop, std::string_view val) {
    if (prop == "visibility") {
        style.visibility = (val == "hidden") ? Visibility::Hidden : Visibility::Visible;
    } else if (prop == "overflow") {
        if (val == "hidden") {
            style.overflow = Overflow::Hidden;
        } else if (val == "scroll") {
            style.overflow = Overflow::Scroll;
        } else if (val == "auto") {
            style.overflow = Overflow::Auto;
        } else {
            style.overflow = Overflow::Visible;
        }
    } else {
        return false;
    }
    return true;
}

void apply_declaration(ComputedStyle& style, const css::Declaration& d) {
    const std::string_view prop = d.property;
    const std::string_view val = d.value;
    if (apply_color_props(style, prop, val)) {
        return;
    }
    if (apply_text_props(style, prop, val)) {
        return;
    }
    if (apply_box_props(style, prop, val)) {
        return;
    }
    if (apply_flex_props(style, prop, val)) {
        return;
    }
    apply_other_props(style, prop, val);
}

// ── Inheritance ───────────────────────────────────────────────────────────────

void inherit_from(ComputedStyle& child, const ComputedStyle& parent) {
    child.color = parent.color;
    child.font_weight = parent.font_weight;
    child.font_style = parent.font_style;
    child.text_align = parent.text_align;
    child.white_space = parent.white_space;
    child.visibility = parent.visibility;
}

// ── Style computation ─────────────────────────────────────────────────────────

ComputedStyle compute_style(const dom::Node& node, const ComputedStyle& parent_style,
                            std::span<const dom::Node* const> ancestors,
                            const std::vector<const css::Stylesheet*>& sheets) {
    ComputedStyle style{};
    inherit_from(style, parent_style);

    std::vector<MatchedDecl> matched;
    std::size_t order = 0;
    collect_from_sheet(node, ancestors, ua_stylesheet(), true, order, matched);
    for (const css::Stylesheet* sheet : sheets) {
        collect_from_sheet(node, ancestors, *sheet, false, order, matched);
    }

    const std::string_view inline_val = node.attr("style");
    std::vector<css::Declaration> inline_decls;
    if (!inline_val.empty()) {
        inline_decls = css::parse_inline(inline_val);
        for (const auto& d : inline_decls) {
            matched.push_back({10000000, order++, &d, d.important});
        }
    }

    std::stable_sort(matched.begin(), matched.end(),
                     [](const MatchedDecl& a, const MatchedDecl& b) {
                         if (a.important != b.important) {
                             return static_cast<int>(a.important) < static_cast<int>(b.important);
                         }
                         if (a.specificity != b.specificity) {
                             return a.specificity < b.specificity;
                         }
                         return a.order < b.order;
                     });

    for (const auto& m : matched) {
        if (m.decl != nullptr) {
            apply_declaration(style, *m.decl);
        }
    }

    // SPEC §9: border width is binary in a character grid — any explicit
    // zero width collapses the border to `none` regardless of border-style.
    for (auto& side : style.border) {
        if (!side.width.is_auto && side.width.value == 0) {
            side.style = BorderStyle::None;
        }
    }

    return style;
}

// ── Tree building ─────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-no-recursion)
StyledNode build_node(const dom::Node& node, const ComputedStyle& parent_style,
                      const std::vector<const css::Stylesheet*>& sheets,
                      std::vector<const dom::Node*>& ancestor_stack) {
    StyledNode sn;
    sn.node = &node;
    if (node.kind == dom::NodeKind::Element) {
        sn.style = compute_style(
            node, parent_style,
            std::span<const dom::Node* const>(ancestor_stack.data(), ancestor_stack.size()),
            sheets);
        ancestor_stack.push_back(&node);
        for (const auto& child : node.children) {
            sn.children.push_back(build_node(*child, sn.style, sheets, ancestor_stack));
        }
        ancestor_stack.pop_back();
    } else {
        sn.style = parent_style;
    }
    return sn;
}

}  // namespace

std::optional<StyledNode> resolve(const dom::Document& doc,
                                  std::span<const css::Stylesheet> author_sheets) {
    if (!doc.root) {
        return std::nullopt;
    }
    std::vector<const css::Stylesheet*> sheets;
    sheets.reserve(author_sheets.size());
    for (const auto& s : author_sheets) {
        sheets.push_back(&s);
    }
    std::vector<const dom::Node*> ancestor_stack;
    const ComputedStyle root_initial{};
    return build_node(*doc.root, root_initial, sheets, ancestor_stack);
}

}  // namespace tvshow::style
