# adr-extension-ui-protocol — structured widget commands for window-provider extensions

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-extensions-api

## Context

`adr-external-window-provider`'s v1 protocol is plain text: an input line feeds the child's
stdin, whatever the child prints goes to a scrollback log. That ADR explicitly deferred a
structured protocol — "no real extension exists yet to motivate the shape, premature design."

It's no longer premature. The bundled `calculator`/`calendar` examples were built against the
plain-text protocol and, while functional, render identically to a chat log — not what "a
calculator like with buttons" means. A plain stdin/stdout text pipe *cannot* hand a child process
a clickable widget; only tvshow (the process holding the `tvision` dependency) can draw a
`TButton`. Getting real buttons for an externally-written, arbitrary-language extension requires
the child to *describe* the widget and tvshow to draw it — a small remote-UI protocol.

## Decision

Opt-in, backward compatible: a child's first stdout line, if exactly `UI_INIT`, switches its
`ExtensionWindow` into structured mode. Any child that doesn't print that line keeps today's
plain-scrollback behavior unchanged — this is additive, not a breaking protocol version bump.

Commands (child → tvshow, one per stdout line), scoped to exactly what the two bundled examples
need — a button grid and a text display — not a general layout/styling system:

```
BUTTON id=<int> x=<int> y=<int> w=<int> label="<text>"
TEXT   id=<int> x=<int> y=<int> w=<int> [h=<int>] value="<text>"
CLEAR
TITLE  value="<text>"
```

tvshow → child, on a press: `CLICK id=<int>`. Re-using an `id` in a later `BUTTON`/`TEXT`
updates that widget in place — calculator's running display and calendar's month grid both work
this way, neither needs `CLEAR` for normal operation.

Parsing (`extension_ui_protocol.hpp`/`.cpp`) is a pure module — no `tvision` dependency, tested
headlessly like `net::blocklist`/`util::media_kind`. `ExtensionWindow` applies parsed commands:
new `TButton`s get command id `1000 + ui_id` and `handleEvent` routes `evCommand` in
`[1000, 2000)` back to `CLICK id=<ui_id>`; `TEXT` is backed by a new `LabelView : TView` (plain
`TStaticText` can't have its text changed after construction, which both calculator's display and
calendar's month grid need on every click).

Widget positions are absolute (window content top-left = `0,0`) and don't reflow on resize —
documented v1 limitation, same "extend if a real need shows up" stance as the escaping rules
below.

Quoting: values are double-quoted; `\"`, `\n`, `\\` are the only recognized escapes. No richer
escaping in v1 (matches SPEC §7.4's "ignored properties, extend on demand" pattern) — sufficient
for both bundled examples, revisit if a real extension needs more.

## Consequences

- New pure module `extension_ui_protocol.{hpp,cpp}` + `LabelView` (shared with
  `adr-translator-native-window`'s result field).
- `ExtensionWindow` gains a mode split: `ui_mode_` bool decided by the first line, plain-mode
  chrome (`TInputLine` + scrollback `TListViewer`) torn down via `TObject::destroy` if the child
  opts into structured mode.
- `calculator.py`/`calendar_provider.py` rewritten to speak the protocol instead of plain text —
  same underlying logic (AST-walk evaluator, `calendar.TextCalendar`), different output format.
- No JSON, no schema versioning in v1 — a hand-rolled line grammar is enough for two widget kinds
  and keeps the reference scripts readable without a parsing library dependency.

## Alternatives Considered

- **Keep everything plain-text, build calculator/calendar as native C++ features instead**:
  rejected — loses "any language, no build-time coupling to tvshow" for the two most-wanted
  examples, and doesn't generalize to a third extension someone else writes later.
- **Full remote-UI protocol (arbitrary layout, styling, multi-pane)**: rejected for v1 — same
  "no real motivating use case yet" reasoning the original ADR used; BUTTON/TEXT/CLEAR/TITLE
  covers what's needed today, extend later if a concrete third example demands more.
- **JSON-lines protocol**: rejected — pulls in a JSON parser (or hand-rolls one anyway) for a
  grammar this simple; the flat `key=value` line format needs nothing beyond `std::from_chars`
  and string splitting, and stays trivially writable from a shell script if someone wants one.

## References

- SPEC §20 q-extensions-api
- `adr-external-window-provider` (protocol this extends; plain-scrollback mode is now the
  fallback, not the only mode)
- `adr-translator-native-window` (why translator did *not* adopt this protocol instead of staying
  a bespoke native window)
- `extensions/README.md` (protocol grammar reference for extension authors)
