#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"

#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int kGoldenCols = 40;
constexpr int kGoldenRows = 10;

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << content;
    return out.good();
}

// Renders an HTML string (with optional inline style blocks) to a CharGrid.
tvshow::render::CharGrid render_html(std::string_view html) {
    auto doc = tvshow::dom::parse(html);
    REQUIRE(doc.has_value());

    std::vector<tvshow::css::Stylesheet> sheets;
    for (const auto& css_text : doc->inline_styles) {
        if (auto sheet = tvshow::css::parse(css_text)) {
            sheets.push_back(std::move(*sheet));
        }
    }

    auto tree = tvshow::style::resolve(*doc, sheets);
    REQUIRE(tree.has_value());

    const tvshow::layout::Box box = tvshow::layout::layout(*tree, {kGoldenCols, kGoldenRows});
    return tvshow::render::render(box);
}

// Checks (or updates) a golden snapshot for the given fixture name.
// fixture_name is just the base name without extension, e.g. "minimal".
void check_golden(const std::string& fixture_name) {
    const std::string golden_dir = TVSHOW_GOLDEN_DIR;
    const std::string html_path = golden_dir + "/" + fixture_name + ".html";
    const std::string grid_path = golden_dir + "/" + fixture_name + ".grid";

    const auto html = read_file(html_path);
    REQUIRE(html.has_value());

    const tvshow::render::CharGrid grid = render_html(*html);
    const std::string snapshot = grid.to_string();

    const bool update = (std::getenv("UPDATE_GOLDEN") != nullptr);
    if (update) {
        CHECK(write_file(grid_path, snapshot));
        return;
    }

    const auto stored = read_file(grid_path);
    REQUIRE(stored.has_value());
    CHECK(snapshot == *stored);
}

}  // namespace

// ── Golden fixtures ───────────────────────────────────────────────────────────

TEST_CASE("golden: minimal — plain text renders correctly") {
    check_golden("minimal");
}

TEST_CASE("golden: borders — solid border box-drawing glyphs") {
    check_golden("borders");
}

TEST_CASE("golden: flex_row — flex row layout with grow") {
    check_golden("flex_row");
}
