#!/usr/bin/env python3
"""tvshow window-provider example: a basic calendar.

Speaks the adr-external-window-provider protocol (see calculator.py for the
protocol description). Stdlib only -- no network, no dependencies.

Usage in ~/.config/tvshow/config.toml:
    window-provider-calendar = "/path/to/tvshow/extensions/calendar/calendar_provider.py"

Commands (typed into the extension window's input line):
    today                  -> today's date, weekday, ISO week number
    YYYY-MM                -> text calendar for that month
    YYYY-MM-DD              -> weekday name for that specific date
    help                   -> usage
"""

import calendar as cal_module
import datetime
import re
import sys

_MONTH_RE = re.compile(r"^(\d{4})-(\d{1,2})$")
_DATE_RE = re.compile(r"^(\d{4})-(\d{1,2})-(\d{1,2})$")


def handle(cmd: str) -> str:
    cmd = cmd.strip()
    if not cmd or cmd == "help":
        return "commands: today | YYYY-MM | YYYY-MM-DD"

    if cmd == "today":
        today = datetime.date.today()
        iso_year, iso_week, iso_day = today.isocalendar()
        return f"{today.isoformat()} ({today.strftime('%A')}), ISO week {iso_week}"

    m = _DATE_RE.match(cmd)
    if m:
        year, month, day = (int(g) for g in m.groups())
        try:
            d = datetime.date(year, month, day)
        except ValueError as exc:
            return f"error: {exc}"
        return f"{d.isoformat()} is a {d.strftime('%A')}"

    m = _MONTH_RE.match(cmd)
    if m:
        year, month = int(m.group(1)), int(m.group(2))
        if not 1 <= month <= 12:
            return "error: month must be 1-12"
        return cal_module.TextCalendar(firstweekday=0).formatmonth(year, month).rstrip("\n")

    return "error: unrecognized command -- type 'help'"


def main() -> None:
    print("calendar ready -- type 'today', 'YYYY-MM', or 'help'", flush=True)
    for line in sys.stdin:
        print(handle(line), flush=True)


if __name__ == "__main__":
    main()
