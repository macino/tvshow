#include "tvshow/util/month_grid.hpp"

#include <array>
#include <format>

namespace tvshow::util {

namespace {

constexpr std::array<const char*, 12> kMonthNames = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};

bool is_leap(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) noexcept {
    constexpr std::array<int, 12> dim = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) { return 29; }
    return dim[static_cast<size_t>(month - 1)];
}

// Sakamoto's algorithm: 0 = Sunday .. 6 = Saturday, converted to 0 = Monday.
int weekday_mon0(int y, int m, int d) noexcept {
    constexpr std::array<int, 12> t = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) { y -= 1; }
    const int w = (y + y / 4 - y / 100 + y / 400 + t[static_cast<size_t>(m - 1)] + d) % 7;
    return (w + 6) % 7;
}

std::string center(const std::string& s, int width) {
    const int pad = width - static_cast<int>(s.size());
    if (pad <= 0) { return s; }
    const int left = pad / 2;
    const int right = pad - left;
    return std::string(static_cast<size_t>(left), ' ') + s +
           std::string(static_cast<size_t>(right), ' ');
}

}  // namespace

std::string format_month_grid(int year, int month, int today_year, int today_month,
                              int today_day) {
    std::string out;
    out += center(std::format("{} {}", kMonthNames[static_cast<size_t>(month - 1)], year), 20);
    out += "\nMo Tu We Th Fr Sa Su";

    const int first_wd = weekday_mon0(year, month, 1);  // 0 = Monday
    const int ndays = days_in_month(year, month);
    const bool is_current = (year == today_year && month == today_month);

    int day = 1;
    int col = first_wd;
    out += "\n";
    out += std::string(static_cast<size_t>(col) * 3, ' ');
    while (day <= ndays) {
        std::string cell = (is_current && day == today_day) ? std::format("[{}]", day)
                                                             : std::format("{:2}", day);
        out += cell;
        ++col;
        ++day;
        if (col == 7) {
            if (day <= ndays) { out += "\n"; }
            col = 0;
        } else if (day <= ndays) {
            out += " ";
        }
    }
    return out;
}

}  // namespace tvshow::util
