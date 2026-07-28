# adr-persisted-cookies — disk-persisted cookies

- **Status:** accepted
- **Date:** 2026-07-27
- **Touches SPEC:** §20 Q-9, q-persisted-cookies

## Context

Q-9 resolved v1 with an in-memory `CookieJar` (RFC 6265 §5 simplified), scoped to
`SharedBrowsingState`, lost on process exit. q-persisted-cookies asks for persistence across
restarts — closer to a real GUI browser's cookie-jar behavior.

## Decision

`CookieJar` gains `load(path)` / `save(path)` serializing to
`~/.config/tvshow/cookies` (same XDG_CONFIG_HOME-respecting location pattern as `config.toml`
and `bookmarks`, Q-19/Q-2). Format: one cookie per line, tab-separated
(`domain\tpath\tname\tvalue\texpires\tsecure\thttponly`), mirroring the existing bookmarks-file
convention. Loaded at startup (before first request), saved on process exit and after every
`Set-Cookie` write (crash-safe — no in-memory-only window). Session cookies (no `Max-Age`/
`Expires`) are still dropped on exit, matching real-browser semantics — only persistent cookies
are written to disk. No Secure/SameSite enforcement added (still out of scope, unchanged from
Q-9).

## Consequences

- New I/O in `net/` (or wherever `CookieJar` lives) — file read/write, still outside pure
  pipeline stages (net is already marked impure in SPEC §3.1).
- Cookie file is plaintext on disk — same trust model as `bookmarks` (no encryption); worth a
  one-line callout in README/SPEC if it stores anything sensitive.
- Expired-cookie pruning needed on load (skip/drop entries past `expires`).

## Alternatives Considered

- **SQLite cookie store** (like real browsers): rejected — new dependency for a demo-scale
  browser, disproportionate.
- **Persist all cookies incl. session ones**: rejected — breaks session-cookie semantics users
  expect (e.g. re-auth after restart).

## References

- SPEC §20 Q-9, q-persisted-cookies, Q-19 (config file location), Q-2 (bookmarks file convention)
