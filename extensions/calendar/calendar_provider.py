#!/usr/bin/env python3
"""tvshow window-provider example: a calendar with a real navigable month grid.

Speaks the adr-extension-ui-protocol structured UI mode (see calculator.py
for the protocol description and extensions/README.md for the grammar).
Stdlib only -- no network, no dependencies.

Usage in ~/.config/tvshow/config.toml:
    window-provider-calendar = "/path/to/tvshow/extensions/calendar/calendar_provider.py"

Two buttons ("<" / ">") step the displayed month backward/forward; the text
area shows that month's grid, today's cell wrapped in brackets when visible.
"""

import calendar as cal_module
import datetime
import re
import sys

_DISPLAY_ID = 100
_PREV_ID = 1
_NEXT_ID = 2


def esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def month_grid(year: int, month: int) -> str:
    text = cal_module.TextCalendar(firstweekday=0).formatmonth(year, month).rstrip("\n")
    today = datetime.date.today()
    if (year, month) == (today.year, today.month):
        # Wrap today's day number in brackets. Bounded by non-digits on both
        # sides so e.g. day 2 doesn't also match inside "20" or "12".
        text = re.sub(rf"(?<!\d){today.day}(?!\d)", f"[{today.day}]", text, count=1)
    return text


def main() -> None:
    today = datetime.date.today()
    year, month = today.year, today.month

    print("UI_INIT", flush=True)
    print('TITLE value="Calendar"', flush=True)
    print(f'BUTTON id={_PREV_ID} x=0 y=0 w=5 label="<"', flush=True)
    print(f'BUTTON id={_NEXT_ID} x=30 y=0 w=5 label=">"', flush=True)

    def emit_grid() -> None:
        grid = month_grid(year, month)
        rows = grid.count("\n") + 1
        print(f'TEXT id={_DISPLAY_ID} x=0 y=2 w=36 h={rows} value="{esc(grid)}"', flush=True)

    emit_grid()

    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("CLICK id="):
            continue
        try:
            kid = int(line[len("CLICK id="):])
        except ValueError:
            continue

        if kid == _PREV_ID:
            month -= 1
            if month < 1:
                month, year = 12, year - 1
        elif kid == _NEXT_ID:
            month += 1
            if month > 12:
                month, year = 1, year + 1
        else:
            continue
        emit_grid()


if __name__ == "__main__":
    main()
