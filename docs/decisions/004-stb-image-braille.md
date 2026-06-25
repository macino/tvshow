# 004 — stb_image for braille image rendering

- **Status:** accepted
- **Date:** 2026-06-25
- **Touches SPEC:** §6.5, §20 Q-23

## Context

The `ImageRenderer` interface (SPEC §6.5) currently only has `AltTextRenderer` which renders `[alt]` text. M1 adds a braille-dot renderer that needs to decode image pixel data from HTTP-fetched bytes. We need an image decoding library.

## Decision

Vendor `stb_image.h` (v2.30, public domain) under `third_party/stb/`. Header-only, zero build-system changes beyond a single `.cpp` that defines `STB_IMAGE_IMPLEMENTATION`. Decodes PNG, JPEG, BMP, GIF — sufficient for web images.

## Consequences

- `BrailleRenderer` can decode image bytes and quantize to 2x4 braille dot patterns (Unicode U+2800..U+28FF).
- Adds ~7k lines of vendored C code (header-only, compiled in one TU).
- No new CMake `FetchContent` — just a vendored header.

## Alternatives Considered

- **libpng + libjpeg**: two system deps, heavier build integration, format-specific.
- **Fetch from network at render time**: rejected — rendering must stay pure (no I/O).

## References

- https://github.com/nothings/stb
- SPEC §6.5, §20 Q-23
