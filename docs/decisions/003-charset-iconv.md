# 003 — Charset transcoding via POSIX iconv

- **Status:** accepted
- **Date:** 2026-06-23
- **Touches SPEC:** §15.1, Q-8

## Context

HTML served with non-UTF-8 charsets (ISO-8859-x, Windows-125x) renders as garbled bytes when fed
directly to Gumbo's UTF-8 parser. Content-Type headers and `<meta charset>` tags carry the encoding
declaration; we must transcode to UTF-8 before parsing.

## Decision

Use POSIX `iconv(3)` (part of glibc, no new dependency) to transcode response bodies to UTF-8.
Priority: Content-Type header charset > `<meta charset>` prescan > assume UTF-8.

The prescan reads the first 2048 bytes before Gumbo parses, searching for the literal string
`charset=` case-insensitively — sufficient for all common `<meta charset>` and
`<meta http-equiv="Content-Type">` patterns.

Module: `util::charset` — `transcode_to_utf8(src, from_charset)` and `prescan_charset(html_bytes)`.
Applied in `app::fetch_http()` after each HTTP response.

## Consequences

- **Positive:** Latin-1 / Windows-1252 / ISO-8859-x pages render correctly.
- **Positive:** iconv is in glibc on Linux and system libc on macOS — zero build-time cost.
- **Neutral:** Unsupported charsets fall through silently (iconv returns -1 on open; we return raw bytes).
- **Negative:** Prescan is not a full HTML5 encoding sniff; edge cases with BOM or complex meta ordering are not handled.

## Alternatives Considered

- **simdutf / ICU**: full Unicode normalization, but a new compiled dep requiring an ADR. Overkill for charset transcoding alone.
- **Custom table**: manual ISO-8859-1→UTF-8 table. Only covers one encoding; not worth the maintenance.

## References

- SPEC §15.1 (charset handling)
- HTML5 §12.2.2 (encoding determination algorithm)
- POSIX iconv(3) man page
