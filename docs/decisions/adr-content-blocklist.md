# adr-content-blocklist — config-driven content/ad blocking

- **Status:** proposed
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-extensions-api, §3.1 (net/style pipeline stages)

## Context

q-extensions-api names content/ad blocking as a wanted 3rd-party-extendable capability. Real ad
blockers ship as rule lists (URL patterns, element-hide selectors) consumed by the browser, not
executable plugins. That model needs no code-execution surface at all — the safest of the three
extension mechanisms considered for q-extensions-api.

## Decision

`~/.config/tvshow/blocklist` (XDG_CONFIG_HOME-respecting, same location pattern as `bookmarks`/
`cookies`, Q-19/Q-2), one rule per line, two rule kinds distinguished by prefix:
- `block: <url-glob>` — checked in `net::HttpClient` before a request is issued (matched request
  short-circuits to a synthetic empty response, same shape as a failed fetch — existing
  degrade-gracefully path, no new error handling needed).
- `hide: <css-selector>` — applied during `style/` cascade resolution as an implicit
  `display: none` rule, lowest specificity (page/user CSS can still override if they explicitly
  target the same selector — blocklist is a default, not a hard override).

3rd parties distribute a `blocklist` file; user drops it in place (or a future `--blocklist=path`
CLI flag points at one). No fetching of remote block lists in v1 (that would add network I/O to
startup and a trust/update-cadence question out of scope here).

## Consequences

- `net::HttpClient` gains a pre-request glob-match check against loaded rules — pure function,
  testable without a real request.
- `style/` cascade gains one synthetic lowest-specificity rule source, ahead of UA stylesheet
  (Q-4) in priority order.
- New file format, but reuses the existing tab-separated/line-based convention already used by
  `bookmarks` and `cookies` — no new parser paradigm introduced.
- No remote list fetching/updating — user-managed file only, matches project's no-persistent-
  network-dependency posture.

## Alternatives Considered

- **Fetch and auto-update from a remote blocklist URL**: rejected — adds network I/O at startup,
  a caching/update-cadence question, and a trust decision (whose list?) out of scope for v1.
- **JS-based blocking (like uBlock's cosmetic filtering engine)**: moot — no JS engine exists
  (§1.2 non-goal).

## References

- SPEC §20 q-extensions-api, Q-19, Q-2, Q-4
