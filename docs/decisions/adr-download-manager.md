# adr-download-manager — "Save Link As" download support

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-download-manager, Q-28 (file upload precedent), Q-19 (config file)

## Context

q-download-manager: no way to write a fetched resource to disk. File input (Q-28) is upload-only.
Real GUI browsers offer "Save Link As" / "Save Page As" plus a download-progress/history view;
full parity is disproportionate for this project's scope (§1.1 — hostable demo stack, solo
maintainer).

## Decision

Add "Save Link As" on the focused link/media-placeholder token (menu entry + shortcut, mirrors
existing Enter-to-navigate focus model). Opens `TFileDialog` pre-filled with the last-used save
directory. Fetch is already synchronous (existing `net::HttpClient` call path) — no progress bar,
no background queue, no download history list; write completes or fails inline, same
degrade-gracefully pattern as the rest of net/ (failure = status-line message, no crash).

Last-used directory persists to `~/.config/tvshow/config.toml` (`download-dir` key, same
XDG_CONFIG_HOME-respecting location as `bookmarks`/`cookies`, Q-19/Q-2) — survives restart, not
just within-session, consistent with how bookmarks/cookies already persist. Updated on every
successful save.

## Consequences

- New config key `download-dir`; `Config` loader/writer (Q-19) gains one field.
- New menu/shortcut wiring in `ui/` + a `save_link_as()` path in `app/browser_view.cpp` alongside
  existing `show_file_picker()` (Q-28) — same `TFileDialog` dependency, no new one.
- No download manager window/list — out of scope, matches project size.

## Alternatives Considered

- **Persistent download history + progress UI**: rejected — full parity with GUI browsers,
  disproportionate for a solo demo-scale project.
- **Session-only last-path (not persisted to config)**: rejected — user asked for persistence
  across dialog openings, and the config file is already the established persistence mechanism.

## References

- SPEC §20 q-download-manager, Q-28, Q-19, Q-2
