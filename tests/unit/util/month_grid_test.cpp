#include "tvshow/util/month_grid.hpp"

#include <doctest/doctest.h>

using tvshow::util::format_month_grid;

TEST_CASE("format_month_grid: header and weekday row") {
    const auto grid = format_month_grid(2026, 7, /*today*/ 1970, 1, 1);
    CHECK(grid.find("July 2026") != std::string::npos);
    CHECK(grid.find("Mo Tu We Th Fr Sa Su") != std::string::npos);
}

TEST_CASE("format_month_grid: July 2026 starts on a Wednesday") {
    // 2026-07-01 is a Wednesday -> first two week-slots (Mo, Tu) are blank.
    const auto grid = format_month_grid(2026, 7, 1970, 1, 1);
    const auto nl = grid.find('\n', grid.find('\n') + 1);
    const auto week1 = grid.substr(nl + 1, grid.find('\n', nl + 1) - nl - 1);
    CHECK(week1 == "       1  2  3  4  5");
}

TEST_CASE("format_month_grid: brackets today's day when in the displayed month") {
    const auto grid = format_month_grid(2026, 7, 2026, 7, 28);
    CHECK(grid.find("[28]") != std::string::npos);
}

TEST_CASE("format_month_grid: no brackets when today is a different month") {
    const auto grid = format_month_grid(2026, 6, 2026, 7, 28);
    CHECK(grid.find('[') == std::string::npos);
}

TEST_CASE("format_month_grid: February in a leap year has 29 days") {
    const auto grid = format_month_grid(2024, 2, 1970, 1, 1);
    CHECK(grid.find("29") != std::string::npos);
    CHECK(grid.find("30") == std::string::npos);
}

TEST_CASE("format_month_grid: February in a non-leap year has 28 days") {
    const auto grid = format_month_grid(2026, 2, 1970, 1, 1);
    CHECK(grid.find("28") != std::string::npos);
    CHECK(grid.find("29") == std::string::npos);
}
