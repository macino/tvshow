# adr-native-demo-windows — native Calculator/Calendar/Puzzle/ASCII Chart/Event Viewer

- **Status:** accepted
- **Date:** 2026-07-29
- **Touches SPEC:** §20 q-extensions-api, §11 (UI Model)

## Context

The subprocess-based calculator/calendar (`adr-extension-ui-protocol`) technically worked but
didn't match what was actually wanted: a reference screenshot of `magiblot/tvision`'s own bundled
`examples/tvdemo` (Calculator, Calendar, Puzzle, ASCII Chart, Event Viewer — a green keypad + blue
display, a proper month grid, a sliding-tile puzzle, a character table, a live event-struct dump)
made the ask concrete. tvision already vendors that exact demo source
(`build/_deps/tvision-src/examples/tvdemo/{calc,puzzle}.cpp`, `tvdemo{1,2,3}.cpp`) as a build
dependency — but those files carry a `Copyright (c) 1994 by Borland International, All Rights
Reserved` header, not the MIT terms the tvision *library* itself ships under. Copying them
verbatim into this MIT-licensed repo would carry that provenance forward without a clean
relicensing grant.

## Decision

Five clean-room native `TWindow`s, written fresh against this project's own conventions (not
ported from Borland's source) — inspired by the screenshot's look and interaction model only:

- **`CalculatorWindow`** — a `TButton` keypad (`C ( ) / · 7 8 9 * · 4 5 6 - · 1 2 3 + · 0 . ⌫ =`)
  and a right-aligned `LabelView` display. Math is `util::evaluate_arith` — a small hand-rolled
  recursive-descent parser (`+ - * /`, parens, unary minus), no `eval()`-equivalent, no exceptions
  across the module boundary (`std::optional<double>`, matching this codebase's existing
  `Url::parse`-style convention rather than introducing a new result-type pattern).
- **`CalendarWindow`** — `util::format_month_grid` (pure, deterministic — takes today's date as a
  parameter rather than reading the clock, so it stays testable) plus "<"/">" `TButton`s.
- **`PuzzleWindow`** — 4×4 sliding tiles, letters A–O + blank, shuffled via 200 random legal moves
  from the solved state at startup (guarantees solvability without needing a separate solver).
  `TButton::title` is reassigned in place on every move (it's a public `const char*`, same
  technique `ExtensionWindow`'s `TITLE` command uses) rather than destroying/recreating 16 buttons
  per click.
- **`AsciiChartWindow`** — a custom `TView` wrapping the printable ASCII range (0x20–0x7E) at
  whatever width the window is, a status `LabelView` showing decimal/hex for whatever character
  was clicked. Deliberately printable-ASCII-only, not the extended code-page-437 glyphs the
  screenshot shows — those need a font/encoding mapping this project doesn't otherwise touch, and
  "ASCII Chart" is honestly named for the subset actually rendered.
- **`EventViewerWindow`** — logs the raw `TEvent` structs (mouse/keyboard) *this window* receives
  to a scrollback list. Scoped to one window's events, not a whole-application hook: the real
  tvdemo patches the event loop itself to see everything; doing that here would be a much larger
  change than a demo window justifies. Move the mouse over it / type while it's focused to see
  events logged.

`LabelView` (added for `adr-extension-ui-protocol`/`adr-translator-native-window`) gained a
`TColorAttr` constructor param and `set_right_align()` to support the calculator's blue,
right-aligned display — both optional, existing call sites unaffected.

All five reachable from the **Tools** menu (alongside Translate), no default Ctrl-shortcuts —
the useful single-letter combos are already spoken for elsewhere in the app.

**Relationship to the subprocess mechanism**: `adr-extension-ui-protocol`'s `BUTTON`/`TEXT`/`CLICK`
protocol and the window-provider calculator/calendar reference scripts are *not* removed — they
remain a working demonstration of that protocol for anyone writing a genuinely 3rd-party (not
bundled-with-tvshow) extension in another language. `extensions/README.md` now says so explicitly:
native is what tvshow itself uses today; the scripts stay as protocol reference material.

**Sizing and color, round two**: the first version of these windows used `Application::
next_window_bounds()` (the same "cascade + fill remaining desktop" sizing `BrowserWindow` uses)
and left window backgrounds at `TWindow`'s default blue — both wrong for content-sized dialogs,
confirmed by direct comparison against the reference screenshot once this environment turned out
to have a usable `DISPLAY` (see below). Fixed with `Application::fixed_window_bounds(w, h)` — a
sibling of `next_window_bounds()` that centers a *fixed*-size window with the same per-open
stagger, instead of stretching to the desktop.

Button color took a second pass to get right: `TWindow::palette = wpGrayWindow` (etc.) alone
*doesn't* recolor `TButton`s — `wpGrayWindow` selects `cpGrayWindow`, only 8 palette entries, but
`TButton`'s own local palette (`cpButton`) indexes up to entry 15 into its owner's palette. Indices
that fall outside the owner's table don't cleanly error, they just silently resolve to some
default color one level further up the chain — which is what produced the plain-red buttons this
ADR's first draft shipped with, unnoticed because rendering hadn't been visually checked at all
yet. The fix: override `getPalette()` directly to return `cpGrayDialog`/`cpCyanDialog`/
`cpBlueDialog` (32 entries each) — the same tables `TDialog` itself uses — even though these are
plain `TWindow`s, not `TDialog`s. Long enough for `TButton`'s indices, and it turns out the
"button" palette slot resolves to green in all three dialog color families, matching the reference
screenshot without any additional per-button color code.

**Keyboard input and Char Chart, round three**: buttons-only interaction was flagged as wrong for
a keyboard-first TUI. `CalculatorWindow` now handles `evKeyDown` (digits, operators, `Enter`→`=`,
`Backspace`→`⌫`, `Esc`/`c`/`C`→`C`) via the same `press(key_id)` path the buttons already used —
verified interactively via `send-text` keystrokes (`7*8` → `56`); an early false alarm ("78"
instead of "56", then "error" for any 3+-char burst) turned out to be `kitty send-text`'s
bracketed-paste handling losing/mangling multi-character bursts, not a real bug — sending the same
keys one at a time confirmed the C++ logic was correct throughout. `CalendarWindow` gained
`Left`/`Right` (prev/next month, mirroring the existing buttons) and `Up`/`Down` (prev/next year,
new) plus a `TInputLine` year field for direct jump-to-year on `Enter`. Two `TInputLine` quirks
surfaced here worth remembering: the ctor's `limit` argument is `maxLen + 1` (a byte count
including the null terminator, not the visible character count — passing `kYearFieldW` directly
silently dropped the last digit), and an exact-width box (`limit` columns) reserves space for
scroll-indicator arrows and hides content, so the box needs `maxLen + 2` columns even when the
text always fits.

`AsciiChartWindow` was renamed `CharChartWindow` ("Char Chart" in the Tools menu, hotkey `H` since
`C` was already `Calculator`'s) and now renders any of eight single-column-width Unicode blocks
(ASCII, Latin-1 Supplement, Latin Extended-A, Greek and Coptic, Cyrillic, General Punctuation, Box
Drawing, Braille Patterns) cycled via new "<"/">" buttons or `Left`/`Right` keys, reusing
`Calendar`'s prev/next layout convention. Wide/CJK/emoji ranges are deliberately excluded — tvision's
`CharGrid` is one cell per column, and a double-width glyph in a single cell misaligns every
character after it on that row; that's a structural limit, not a v1 scoping shortcut worth working
around quietly. Rendering switched from single-byte `char`/`putChar` to `char32_t` codepoints
UTF-8-encoded into a string per row and painted with `TDrawBuffer::moveStr` (the same UTF-8-aware
primitive `LabelView::draw()` already relies on) instead of the old byte-at-a-time `putChar` loop.
Screenshot testing while cycling blocks caught one more real bug: `ChartView::draw()` originally
only repainted rows up to the new block's character count, so switching from a larger block (e.g.
Cyrillic, 256 chars) to a smaller one (General Punctuation, 112 chars) left the previous block's
glyphs on screen in the rows the new pass no longer touched. Fixed by always repainting the full
view height, blank-padding rows past the current block's last character.

## Consequences

- New `util::evaluate_arith` / `util::format_month_grid` — pure, unit-tested (`arith_eval_test`,
  `month_grid_test`).
- Five new `TWindow` subclasses in `app/`, five new `cmOpen*` commands, five new Tools-menu items.
- `LabelView` gains two small, backward-compatible extension points.
- New `Application::fixed_window_bounds(w, h)`, alongside the existing `next_window_bounds()`.
- `CalculatorWindow`/`CalendarWindow`/`PuzzleWindow` override `getPalette()` (`cpGrayDialog`/
  `cpCyanDialog`/`cpBlueDialog`) instead of the shorter `TWindow::palette` field selector.
- No new vendored dependency, no license provenance question — everything here is fresh code.
- This environment turned out to have a live `DISPLAY` and the tooling `scripts/take-screenshots.sh`
  already uses (kitty + `xwininfo` + ImageMagick `import`) — should have been checked *before*
  claiming these windows were verified "at the logic level only." Once checked, direct screenshot
  comparison against the reference caught two real bugs in one pass (missing-content-contrast
  display, and the sizing/palette issues this section describes) that pure unit tests structurally
  can't catch, because they're about what a human sees, not what a function returns. Logic-level
  tests (arithmetic, month grid, puzzle solvability-by-construction) remain what's unit-tested;
  layout/color is now also screenshot-verified, not just claimed.

## Alternatives Considered

- **Port Borland's actual tvdemo source**: rejected — see Context; the copyright header makes this
  a worse default than writing fresh code that happens to look similar.
- **Build these as window-provider extensions using `adr-extension-ui-protocol`**: considered,
  since the protocol already exists — rejected because a puzzle's 16 buttons updating in place, a
  live scrolling event log, and a mouse-click-to-inspect character grid all want tighter,
  lower-latency interaction than a subprocess pipe naturally gives, and none of them need to be
  written in a different language than tvshow itself.

## References

- SPEC §20 q-extensions-api, §11
- `adr-extension-ui-protocol`, `adr-external-window-provider` (mechanism these deliberately don't
  use, and why — same reasoning `adr-translator-native-window` used)
- `build/_deps/tvision-src/examples/tvdemo/` (visual/behavioral reference only, not a source)
