#include "tvshow/app/page.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

static std::string cp_to_utf8(char32_t cp) {
    if (cp < 0x80U) {
        return {static_cast<char>(cp)};
    }
    if (cp < 0x800U) {
        return {static_cast<char>(0xC0U | (cp >> 6U)),
                static_cast<char>(0x80U | (cp & 0x3FU))};
    }
    if (cp < 0x10000U) {
        return {static_cast<char>(0xE0U | (cp >> 12U)),
                static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)),
                static_cast<char>(0x80U | (cp & 0x3FU))};
    }
    return {static_cast<char>(0xF0U | (cp >> 18U)),
            static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)),
            static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)),
            static_cast<char>(0x80U | (cp & 0x3FU))};
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: tvshow-capture URL [COLS [ROWS]]\n";
        return 1;
    }
    const std::string url = argv[1];
    const int cols = argc >= 3 ? std::atoi(argv[2]) : 80;
    const int rows = argc >= 4 ? std::atoi(argv[3]) : 24;

    auto page = tvshow::app::load_page(url, {cols, rows});
    if (!page) {
        std::cerr << "Failed to load: " << url << '\n';
        return 1;
    }

    const tvshow::render::CharGrid grid = tvshow::render::render(page->box);

    // Header: URL and link summary
    const auto links = tvshow::layout::collect_links(page->box);
    std::cout << "URL:   " << page->url << '\n';
    std::cout << "Title: " << page->doc.title << '\n';
    std::cout << "Size:  " << grid.cols() << "x" << grid.rows() << '\n';
    std::cout << "Links: " << links.size() << '\n';
    std::cout << std::string(static_cast<size_t>(cols), '-') << '\n';

    for (int row = 0; row < grid.rows(); ++row) {
        for (int col = 0; col < grid.cols(); ++col) {
            std::cout << cp_to_utf8(grid.at({col, row}).cp);
        }
        std::cout << '\n';
    }
    std::cout << std::string(static_cast<size_t>(cols), '-') << '\n';

    // Color sample — find first non-space cell per row.
    std::cout << "Color samples:\n";
    for (int sr = 0; sr < std::min(grid.rows(), 45); ++sr) {
        for (int sc = 0; sc < grid.cols(); ++sc) {
            const auto c = grid.at({sc, sr});
            if (c.cp > U' ') {
                std::cout << "  row " << sr << " col " << sc << ": bg=#"
                          << std::hex << std::setfill('0') << std::setw(6) << c.attr.bg
                          << " fg=#" << std::setw(6) << c.attr.fg
                          << " cp=U+" << std::setw(4) << static_cast<unsigned>(c.cp)
                          << std::dec << '\n';
                break;
            }
        }
    }

    // Link inventory
    if (!links.empty()) {
        std::cout << "Links found:\n";
        for (size_t i = 0; i < links.size(); ++i) {
            const auto& lnk = links[i];
            if (!lnk.spans.empty()) {
                std::cout << "  [" << i << "] href=" << lnk.href
                          << " at row=" << lnk.spans[0].origin.row
                          << " col=" << lnk.spans[0].origin.col << '\n';
            }
        }
    }

    return 0;
}
