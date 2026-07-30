#include "tvshow/app/calendar_window.hpp"

#include "tvshow/util/month_grid.hpp"

#define Uses_TButton
#define Uses_TDialog
#define Uses_TEvent
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TLabel
#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <format>

namespace tvshow::app {

namespace {
constexpr unsigned short cmPrevMonth = 4001;
constexpr unsigned short cmNextMonth = 4002;
// Grid is exactly "Mo Tu We Th Fr Sa Su" wide (20 cols) -- the old 36-wide
// layout just had the ">" button parked far out in empty space left over
// from an earlier, needlessly large default window size.
constexpr int kGridW = 20;
constexpr int kBtnW = 5;
constexpr int kBtnRowH = 2;
constexpr int kYearRowY = kBtnRowH;  // row right below the </> buttons
constexpr int kYearFieldW = 4;
constexpr int kGridY0 = kYearRowY + 2;  // year row + one blank row

void set_year_field(TInputLine* field, int year) {
    std::array<char, kYearFieldW + 1> buf{};
    const auto formatted = std::format("{:d}", year);
    std::copy_n(formatted.data(), std::min(formatted.size(), buf.size() - 1), buf.data());
    field->setData(buf.data());
}
}  // namespace

CalendarWindow::CalendarWindow(const TRect& bounds)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Calendar", wnNoNumber) {
    const auto now = std::chrono::system_clock::now();
    const auto today = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day ymd{today};
    year_ = static_cast<int>(ymd.year());
    month_ = static_cast<unsigned>(ymd.month());

    const TRect inner = getExtent().grow(-1, -1);
    insert(new TButton(TRect{inner.a.x, inner.a.y, inner.a.x + kBtnW, inner.a.y + kBtnRowH}, "<",
                       cmPrevMonth, bfNormal));
    insert(new TButton(
        TRect{inner.a.x + kGridW - kBtnW, inner.a.y, inner.a.x + kGridW, inner.a.y + kBtnRowH},
        ">", cmNextMonth, bfNormal));

    insert(new TLabel(
        TRect{inner.a.x, inner.a.y + kYearRowY, inner.a.x + 5, inner.a.y + kYearRowY + 1},
        "Year:", nullptr));
    // Box is wider than kYearFieldW (max chars) -- an exact-width TInputLine
    // reserves columns for scroll arrows and ends up hiding digits. The
    // `limit` ctor arg is maxLen+1 (it's a byte-count including the null
    // terminator, per TInputLine::TInputLine's `limit - 1` -- not the raw
    // char count the "kYearFieldW" name suggests).
    year_input_ = new TInputLine(
        TRect{inner.a.x + 6, inner.a.y + kYearRowY, inner.a.x + 6 + kYearFieldW + 2,
              inner.a.y + kYearRowY + 1},
        kYearFieldW + 1);
    insert(year_input_);
    set_year_field(year_input_, year_);

    // White on cyan -- matches the reference (no separate black box; the grid
    // text sits directly on the same teal-family background as the window).
    grid_ = new LabelView(
        TRect{inner.a.x, inner.a.y + kGridY0, inner.a.x + kGridW, inner.b.y}, TColorAttr(0x3F));
    insert(grid_);

    update_grid();
}

void CalendarWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    // Content is laid out at fixed absolute positions (no reflow on resize,
    // adr-native-demo-windows' documented v1 limitation) -- shrinking below
    // what it needs doesn't clip gracefully, it lets the "<"/">" buttons
    // poke out past the frame instead. Floor the size so that can't happen;
    // +2 each side accounts for the window frame.
    min.x = std::max(min.x, kGridW + 2);
    min.y = std::max(min.y, kGridY0 + 8 + 2);  // 8 = tallest month grid (header*2 + 6 week rows)
}

void CalendarWindow::update_grid() {
    const auto now = std::chrono::system_clock::now();
    const auto today = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day ymd{today};
    grid_->set_text(util::format_month_grid(year_, month_, static_cast<int>(ymd.year()),
                                            static_cast<unsigned>(ymd.month()),
                                            static_cast<unsigned>(ymd.day())));
    set_year_field(year_input_, year_);
    year_input_->drawView();
}

void CalendarWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);
    if (event.what == evCommand) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        if (event.message.command == cmPrevMonth) {
            --month_;
            if (month_ < 1) { month_ = 12; --year_; }
            update_grid();
            clearEvent(event);
        } else if (event.message.command == cmNextMonth) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
            ++month_;
            if (month_ > 12) { month_ = 1; ++year_; }
            update_grid();
            clearEvent(event);
        }
        return;
    }
    if (event.what == evKeyDown) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const ushort code = event.keyDown.keyCode;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        if (code == kbLeft) {
            --month_;
            if (month_ < 1) { month_ = 12; --year_; }
            update_grid();
            clearEvent(event);
        } else if (code == kbRight) {
            ++month_;
            if (month_ > 12) { month_ = 1; ++year_; }
            update_grid();
            clearEvent(event);
        } else if (code == kbUp) {
            --year_;
            update_grid();
            clearEvent(event);
        } else if (code == kbDown) {
            ++year_;
            update_grid();
            clearEvent(event);
        } else if (code == kbEnter && current == year_input_) {
            std::array<char, kYearFieldW + 1> buf{};
            year_input_->getData(buf.data());
            const int y = std::atoi(buf.data());
            if (y > 0) {
                year_ = y;
                update_grid();
            }
            clearEvent(event);
        }
    }
}

TPalette& CalendarWindow::getPalette() const {
    // cpCyanDialog, not wpCyanWindow -- see CalculatorWindow::getPalette()
    // for why the short window palette isn't long enough for TButton.
    static TPalette pal(cpCyanDialog, sizeof(cpCyanDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
