#include "tvshow/layout/links.hpp"

#include "tvshow/layout/box.hpp"
#include "tvshow/layout/inline_text.hpp"
#include "tvshow/layout/types.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace tvshow::layout {

namespace {

// Splits one href-run of placed tokens [run.begin, run.end) into one
// CellRect per visual row (so a link wrapped across lines yields multiple
// spans for a single Link).
std::vector<CellRect> spans_for_run(const std::vector<PlacedToken>& placed, size_t begin,
                                    size_t end) {
    std::vector<CellRect> spans;
    size_t k = begin;
    while (k < end) {
        const int row = placed[k].pos.row;
        const int col_start = placed[k].pos.col;
        int col_end = col_start;
        while (k < end && placed[k].pos.row == row && placed[k].pos.col == col_end) {
            col_end = placed[k].pos.col + 1;
            ++k;
        }
        spans.push_back({{col_start, row}, {col_end - col_start, 1}});
    }
    return spans;
}

void collect_links_in_box(const std::vector<PlacedToken>& placed, std::vector<Link>& links) {
    size_t i = 0;
    while (i < placed.size()) {
        const std::string_view href = placed[i].token.href;
        if (href.empty()) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < placed.size() && placed[j].token.href == href) {
            ++j;
        }
        links.push_back({href, spans_for_run(placed, i, j)});
        i = j;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_links_rec(const Box& box, std::vector<Link>& links) {
    if (box.node != nullptr) {
        collect_links_in_box(place_inline(*box.node, box.content_box), links);
    }
    for (const auto& child : box.children) {
        collect_links_rec(child, links);
    }
}

}  // namespace

std::vector<Link> collect_links(const Box& root) {
    std::vector<Link> links;
    collect_links_rec(root, links);
    return links;
}

}  // namespace tvshow::layout
