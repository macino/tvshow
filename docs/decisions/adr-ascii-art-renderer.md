# adr-ascii-art-renderer — ASCII-art image renderer mode

- **Status:** accepted
- **Date:** 2026-07-27
- **Touches SPEC:** §6.5, §20 Q-23, q-ascii-art-renderer

## Context

Q-23 resolved v1/M1 with two `ImageRenderer` impls: `AltTextRenderer` (default) and
`BrailleRenderer` (Unicode 2x4-dot glyphs, needs a terminal font with braille block support).
q-ascii-art-renderer asks for a third mode: classic ASCII-art (plain 7-bit chars, works on any
terminal/font, coarser resolution than braille).

## Decision

Add `AsciiArtRenderer : ImageRenderer` (SPEC §6.5 interface, same as `BrailleRenderer`). Reuses
the existing `stb_image.h` decode path (ADR-004) — same `Page::images` cache, no new dependency.
Quantizes each cell to one of a fixed luminance-ramp charset (` .:-=+*#%@`, 10 levels) instead of
braille dot patterns — 1 cell = 1 pixel-block average, coarser than braille's 2x4 sub-cell
resolution but zero font-support requirement. Selected via
`image-renderer = "alt" | "braille" | "ascii"` in `config.toml`, or `--image-renderer=ascii` CLI
flag — same switch Q-23 already defined, just a third enum value.

## Consequences

- No new vendored dependency — reuses `stb_image.h` decode already in tree.
- One new renderer class + luminance-ramp lookup table (pure, testable without terminal).
- Image fetch/decode still gated on non-`alt` selection (existing cost-avoidance behavior from
  Q-23 carries over unchanged).
- Coarser detail than braille at same cell budget — expected, documented tradeoff for
  font-compatibility.

## Alternatives Considered

- **Replace braille with ascii-art**: rejected — different tradeoff (compatibility vs detail),
  no reason to drop either; both are cheap given shared decode path.
- **Dithering (Floyd-Steinberg) before quantization**: nicer output, deferred — adds complexity,
  not requested.

## References

- SPEC §6.5, §20 Q-23, q-ascii-art-renderer
- ADR-004 (stb_image vendoring)
