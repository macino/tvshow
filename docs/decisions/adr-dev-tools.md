# adr-dev-tools — network log overlay

- **Status:** proposed
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-dev-tools, Q-12, Q-22 (existing debug overlay)

## Context

q-dev-tools: today's only introspection is Ctrl-D (box outlines + focus-order labels, Q-12/Q-22).
No console, no network log, no live DOM tree. A full DOM/console devtools pane is disproportionate
(no JS engine exists — nothing to console.log from) and duplicates work the golden-test suite
already covers for layout correctness (CLAUDE.md Test-First Discipline).

## Decision

Extend, don't replace: add a Network Log overlay (separate toggle, e.g. Ctrl-Shift-D or a menu
item) listing requests for the active tab's session — method, URL, status code, byte count,
elapsed ms — newest first, capped ring buffer (e.g. last 50). Sourced from `net::HttpClient` call
sites already in `net/` (wrap/observe, don't restructure). Rendered as a plain-text `TWindow`
overlay, same visual language as existing debug overlay (Q-12). No live DOM tree, no console —
neither has a consumer in a JS-less browser.

## Consequences

- New `RequestLog` ring-buffer type, pure/testable (records tuples, no I/O itself).
- `net::HttpClient` call sites gain a log-append hook — thin, no behavior change to requests
  themselves.
- New `TWindow` overlay in `ui/` + shortcut/menu wiring — same pattern as existing debug overlay.
- Scope explicitly excludes: JS console (no JS), live DOM editing (no mutation model exists
  post-render), request replay/blocking (no interception point in current pipeline).

## Alternatives Considered

- **Full DOM inspector tree view**: rejected — Box Tree is already inspectable via existing Ctrl-D
  box-outline overlay (dimensions shown per box); a separate tree view duplicates that for little
  gain at this project's scope.
- **Log to file instead of in-app overlay**: rejected — breaks the "no terminal needed to verify
  behavior" test philosophy but more importantly doesn't help interactive debugging while using
  the app, which is the actual use case.

## References

- SPEC §20 q-dev-tools, Q-12, Q-22
