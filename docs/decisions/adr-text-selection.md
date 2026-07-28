# adr-text-selection — text range-selection + clipboard copy

- **Status:** accepted
- **Date:** 2026-07-27
- **Touches SPEC:** §20 Q-5, q-text-selection

## Context

Q-5 resolved v1 with link-only copy (Ctrl-C copies focused link URL via OSC 52). No way to
select and copy arbitrary rendered text (paragraph, table cell, etc.) — a baseline GUI-browser
feature. q-text-selection extends scope to add it.

## Decision

Add mouse-drag and keyboard range-selection over rendered `CharGrid` cells. Selection is a
`(start_row, start_col) .. (end_row, end_col)` pair in viewport-cell space, tracked per
`BrowserView`. Selected cells get a distinct `TColorAttr` (reverse-video, same mechanism as
`:focus`). Ctrl-C: if a selection is active, copy the plain-text content of the selected cell
range via OSC 52 (existing clipboard path); else fall back to current link-URL-copy behavior.
Selection is cleared on scroll, resize, or navigation.

## Consequences

- New state on `BrowserView`: selection anchor/end, selecting-in-progress flag.
- Mouse-drag handling: extend existing click hit-test (Q-6) to a drag variant.
- Text extraction: need a `CharGrid` → plain-text-range function (pure, testable without terminal).
- No selection across scrolled-out rows in v1 (selection anchored to visible viewport only) —
  matches existing no-horizontal-scroll-per-container limitation (Q-3).
- Does not touch DOM/layout — purely a render/paint + input concern.

## Alternatives Considered

- **Whole-element copy only** (e.g. click a `<p>` to copy it all): simpler, but not how users
  expect selection to work; rejected.
- **Extend focus-order copy to multi-node ranges** instead of cell-based selection: harder to
  reason about visually (no highlight), rejected.

## References

- SPEC §20 Q-5, q-text-selection
- SPEC §8.2 (`:focus` reverse-video precedent)
