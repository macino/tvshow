# adr-external-handlers — mailcap-style fire-and-forget spawn

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-extensions-api, Q-30 (media link-out placeholders)

## Context

q-extensions-api names video playback (once/if image rendering extends to video frames) and
similar "hand off to an external tool" cases as wanted extension points. Vendoring a codec/video
stack in-tree is disproportionate (same reasoning as the existing braille-image decision, ADR-004,
scaled up) — an mpv-style external player is the natural fit. This is the fire-and-forget half of
q-extensions-api; the interactive half is `adr-external-window-provider`.

## Decision

`config.toml` gains a `[handlers]` table mapping a trigger to a shell command template, `%s` =
substituted target (URL or selected text):
```toml
[handlers]
video = "mpv %s"
iframe = "xdg-open %s"
```
On Enter over a media link-out token (Q-30) whose kind has a configured handler, `fork`+`exec`
(not `system()` — avoids shell-injection on untrusted URLs) spawns the command with the URL as an
argument; tvshow does not wait on it, does not manage its window, does not capture output. No
handler configured for that kind → falls back to today's placeholder-link behavior (Q-30)
unchanged. Missing/non-executable command → status-line error, same degrade-gracefully pattern as
the rest of net/.

## Consequences

- New `[handlers]` section in `Config` (Q-19) — string-keyed table, no schema beyond string
  values.
- New `spawn_handler()` in `input/` or `ui/` — `fork`+`execvp` with argument vector built from the
  URL, not shell-interpolated (blocks command injection via a crafted `href`).
- No process supervision, no output capture — the external tool owns its own window/lifecycle
  entirely outside tvshow's process, consistent with `adr-multiprocess-sandboxing`'s stance that
  tvshow itself doesn't do cross-process supervision.
- Zero new vendored dependencies.

## Alternatives Considered

- **`system(3)` with string interpolation**: rejected — shell-injection risk from a
  server-controlled `href`/`src` value; `fork`+`execvp` with an argv array avoids shell parsing
  entirely.
- **Capture and embed player output**: rejected here — that's the `adr-external-window-provider`
  case (interactive, embedded); this ADR is deliberately the simpler fire-and-forget half.

## References

- SPEC §20 q-extensions-api, Q-30, Q-19
- ADR-004 (precedent: avoid vendoring heavy media-decode deps in-tree)
