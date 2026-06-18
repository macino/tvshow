#include "tvshow/layout/inline_text.hpp"

#include "tvshow/dom/node.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::layout {

namespace {

// Decode one UTF-8 code point starting at s[i]; advance i past the bytes consumed.
char32_t utf8_decode_at(std::string_view s, size_t& i) noexcept {
    const auto lead = static_cast<uint8_t>(s[i]);
    if (lead < 0x80U) {
        ++i;
        return static_cast<char32_t>(lead);
    }
    auto cont = [&](size_t pos) -> uint8_t {
        return pos < s.size() ? static_cast<uint8_t>(s[pos]) : 0x80U;
    };
    if ((lead & 0xE0U) == 0xC0U && i + 1 < s.size()) {
        const char32_t cp = ((lead & 0x1FU) << 6U) | (cont(i + 1) & 0x3FU);
        i += 2;
        return cp;
    }
    if ((lead & 0xF0U) == 0xE0U && i + 2 < s.size()) {
        const char32_t cp =
            ((lead & 0x0FU) << 12U) | ((cont(i + 1) & 0x3FU) << 6U) | (cont(i + 2) & 0x3FU);
        i += 3;
        return cp;
    }
    if ((lead & 0xF8U) == 0xF0U && i + 3 < s.size()) {
        const char32_t cp = ((lead & 0x07U) << 18U) | ((cont(i + 1) & 0x3FU) << 12U) |
                            ((cont(i + 2) & 0x3FU) << 6U) | (cont(i + 3) & 0x3FU);
        i += 4;
        return cp;
    }
    ++i;
    return U'?';
}

bool is_inline_descendant(const style::StyledNode& sn) noexcept {
    return sn.style.display != style::Display::None && sn.style.display != style::Display::Block &&
           sn.style.display != style::Display::Flex;
}

// A run of collapsed whitespace not yet emitted, carrying the href of the
// node where the run started (so a space at a link's boundary keeps the
// href of the text it followed, not whatever text happens to follow it).
struct PendingSpace {
    bool active = false;
    std::string_view href;
};

void collect_text_node(std::string_view text, const style::ComputedStyle& st, style::WhiteSpace ws,
                       std::string_view href, std::vector<InlineToken>& out,
                       PendingSpace& pending) {
    size_t i = 0;
    while (i < text.size()) {
        const char32_t cp = utf8_decode_at(text, i);
        if (ws == style::WhiteSpace::Pre) {
            out.push_back({cp, &st, href});
            continue;
        }
        const bool is_ws = cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r';
        if (is_ws) {
            if (!pending.active && !out.empty()) {
                pending.active = true;
                pending.href = href;
            }
            continue;
        }
        if (pending.active) {
            out.push_back({U' ', &st, pending.href});
            pending.active = false;
        }
        out.push_back({cp, &st, href});
    }
}

// SPEC §12.1: focusable links are `<a href>` elements. `href` is the target
// inherited from the nearest enclosing `<a href>`, or empty outside one.
// Mirrors render::paint_text / layout::has_inline_content's recursion
// condition (Text nodes, plus any non-block/flex/none descendant).
// NOLINTNEXTLINE(misc-no-recursion)
void collect_tokens(const style::StyledNode& sn, style::WhiteSpace ws, std::string_view href,
                    std::vector<InlineToken>& out, PendingSpace& pending) {
    for (const auto& child : sn.children) {
        if (child.node == nullptr) {
            continue;
        }
        if (child.node->kind == dom::NodeKind::Text) {
            collect_text_node(child.node->text, child.style, ws, href, out, pending);
        } else if (child.node->tag == "br") {
            pending.active = false;
            out.push_back({U'\n', &child.style, href});
        } else if (is_inline_descendant(child)) {
            std::string_view child_href = href;
            if (child.node->tag == "a") {
                const std::string_view a_href = child.node->attr("href");
                if (!a_href.empty()) {
                    child_href = a_href;
                }
            }
            collect_tokens(child, ws, child_href, out, pending);
        }
    }
}

std::vector<InlineLine> break_pre(const std::vector<InlineToken>& tokens) {
    std::vector<InlineLine> lines;
    InlineLine cur;
    for (const auto& tok : tokens) {
        if (tok.cp == U'\n') {
            lines.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(tok);
        }
    }
    lines.push_back(std::move(cur));
    return lines;
}

// A half-open [begin, end) range of word tokens within the full token stream.
struct WordSpan {
    size_t begin;
    size_t end;
};

// Appends a single hard-broken word: wider than a line, so it fills lines to
// capacity rather than waiting for a space.
void append_oversized_word(const std::vector<InlineToken>& tokens, WordSpan word, int content_w,
                           std::vector<InlineLine>& lines, InlineLine& cur) {
    if (!cur.empty()) {
        lines.push_back(std::move(cur));
        cur.clear();
    }
    for (size_t k = word.begin; k < word.end; ++k) {
        if (static_cast<int>(cur.size()) >= content_w) {
            lines.push_back(std::move(cur));
            cur.clear();
        }
        cur.push_back(tokens[k]);
    }
}

// Appends a word that fits within a line on its own, wrapping to a new line
// first if it doesn't fit after the current one.
void append_word(const std::vector<InlineToken>& tokens, WordSpan word, std::string_view sep_href,
                 int content_w, std::vector<InlineLine>& lines, InlineLine& cur) {
    const size_t word_len = word.end - word.begin;
    const size_t needed = word_len + (cur.empty() ? 0 : 1);
    if (!cur.empty() && static_cast<int>(cur.size() + needed) > content_w) {
        lines.push_back(std::move(cur));
        cur.clear();
    }
    if (!cur.empty()) {
        cur.push_back({U' ', tokens[word.begin].style, sep_href});
    }
    for (size_t k = word.begin; k < word.end; ++k) {
        cur.push_back(tokens[k]);
    }
}

std::vector<InlineLine> break_normal(const std::vector<InlineToken>& tokens, int content_w) {
    std::vector<InlineLine> lines;
    InlineLine cur;
    size_t i = 0;
    std::string_view sep_href;
    while (i < tokens.size()) {
        if (tokens[i].cp == U'\n') {
            lines.push_back(std::move(cur));
            cur.clear();
            sep_href = {};
            ++i;
            continue;
        }
        if (tokens[i].cp == U' ') {
            sep_href = tokens[i].href;
            ++i;
            continue;
        }
        size_t j = i;
        while (j < tokens.size() && tokens[j].cp != U' ' && tokens[j].cp != U'\n') {
            ++j;
        }
        const WordSpan word{i, j};
        if (static_cast<int>(j - i) > content_w) {
            append_oversized_word(tokens, word, content_w, lines, cur);
        } else {
            append_word(tokens, word, sep_href, content_w, lines, cur);
        }
        i = j;
    }
    if (!cur.empty()) {
        lines.push_back(std::move(cur));
    }
    return lines;
}

}  // namespace

std::vector<InlineLine> break_inline(const style::StyledNode& sn, int content_w) {
    if (content_w <= 0) {
        return {};
    }
    const style::WhiteSpace ws = sn.style.white_space;
    std::vector<InlineToken> tokens;
    PendingSpace pending;
    collect_tokens(sn, ws, {}, tokens, pending);
    if (tokens.empty()) {
        return {};
    }
    if (ws == style::WhiteSpace::Pre) {
        return break_pre(tokens);
    }
    if (ws == style::WhiteSpace::Nowrap) {
        return {std::move(tokens)};
    }
    return break_normal(tokens, content_w);
}

std::vector<PlacedToken> place_inline(const style::StyledNode& sn, CellRect content_box) {
    std::vector<PlacedToken> placed;
    const auto lines = break_inline(sn, content_box.size.cols);
    const int max_row = content_box.size.rows;
    const int max_col = content_box.origin.col + content_box.size.cols;
    const style::TextAlign align = sn.style.text_align;
    for (int row = 0; row < static_cast<int>(lines.size()) && row < max_row; ++row) {
        const auto& line = lines[row];
        int col = content_box.origin.col;
        const int slack = content_box.size.cols - static_cast<int>(line.size());
        if (align == style::TextAlign::Center) {
            col += std::max(0, slack / 2);
        } else if (align == style::TextAlign::Right) {
            col += std::max(0, slack);
        }
        for (const auto& tok : line) {
            if (col >= max_col) {
                break;  // clipped — content overflows its box
            }
            placed.push_back({{col, content_box.origin.row + row}, tok});
            ++col;
        }
    }
    return placed;
}

}  // namespace tvshow::layout
