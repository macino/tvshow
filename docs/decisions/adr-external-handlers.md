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

**As implemented** (revised from the original table-based sketch below — our `Config` parser is a
flat `key = "value"` subset with no `[section]` support, so the design had to fit that):
`config.toml` gains flat `handler-video`/`handler-audio` keys, `%s` = substituted absolute URL:
```toml
handler-video = "mpv %s"
handler-audio = "mpv --no-video %s"
```
Dispatch is by **URL file extension** (`util::classify_media_url` — `.mp4`/`.webm`/`.mkv`/`.avi`/
`.mov` → Video, `.mp3`/`.wav`/`.ogg`/`.flac` → Audio), not by DOM tag/"kind" as first sketched:
`layout::Link` carries only `href` + `spans`, no node/tag identity, and threading that through the
whole layout pipeline was a much larger change than the extension-classification heuristic below.
On Enter over a link whose resolved URL classifies as Video/Audio and has a configured handler,
`fork`+`execvp` (not `system()` — avoids shell-injection on untrusted URLs; argv built by
`util::build_handler_argv`, which splits the template on whitespace and substitutes the literal
`%s` token, never interpolating through a shell) spawns the command; tvshow does not wait on it,
does not manage its window, does not capture output. No handler configured, or URL doesn't
classify as media → falls back to normal navigation (Q-30 placeholder behavior for iframe/other
media types is unaffected — `iframe` has no reliable extension to classify by, so it's out of
scope for this mechanism).

**Autonomy note**: this mechanism assumes the configured command (`mpv`, etc.) is already
installed — tvshow has no package manager and won't install one for you. If nothing is
configured, or the command isn't found, the extension window shows a clear failure line rather
than silently doing nothing (see `adr-external-window-provider`'s Autonomy note for the same
principle applied to embedded extensions).

## Consequences

- New `handler-video`/`handler-audio` string keys in `Config` (Q-19) — flat keys, not a table
  (our TOML subset has no section syntax).
- `util::build_handler_argv()` (pure, tested) does the template-splitting; `app::spawn_handler()`
  does the actual `fork`+`execvp` (double-forked so the handler process is reparented to init on
  exit — no zombie, no supervision needed).
- No process supervision, no output capture — the external tool owns its own window/lifecycle
  entirely outside tvshow's process, consistent with `adr-multiprocess-sandboxing`'s stance that
  tvshow itself doesn't do cross-process supervision.
- Zero new vendored dependencies in tvshow itself — but the *handler* is an unvendored runtime
  dependency the user must have installed separately (mpv, etc.). No handlers are configured by
  default; a fresh `config.toml` has none, so this feature is fully opt-in.

## Alternatives Considered

- **`system(3)` with string interpolation**: rejected — shell-injection risk from a
  server-controlled `href`/`src` value; `fork`+`execvp` with an argv array avoids shell parsing
  entirely.
- **Capture and embed player output**: rejected here — that's the `adr-external-window-provider`
  case (interactive, embedded); this ADR is deliberately the simpler fire-and-forget half.

## References

- SPEC §20 q-extensions-api, Q-30, Q-19
- ADR-004 (precedent: avoid vendoring heavy media-decode deps in-tree)
