#pragma once

#include <string>

namespace tvshow::util {

// Text month-grid formatter for the native Calendar window -- Monday-first
// weeks, same layout as Python's calendar.TextCalendar (kept consistent with
// the earlier window-provider calendar_provider.py reference script). Pure,
// no clock reads: today's date is passed in, not read from the system clock,
// so this stays deterministic and testable.
//
// If (today_year, today_month) == (year, month), today_day's cell is wrapped
// in brackets, e.g. "[28]", so the caller doesn't need a second pass.
[[nodiscard]] std::string format_month_grid(int year, int month, int today_year, int today_month,
                                            int today_day);

}  // namespace tvshow::util
