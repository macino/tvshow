# adr-external-window-provider — embedded 3rd-party windows via subprocess pipe

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-extensions-api, §11 (UI Model), §3.1 (module boundaries)

## Context

q-extensions-api names calculator and translation windows as wanted 3rd-party extensions —
unlike `adr-external-handlers`' fire-and-forget case, these need to feel embedded in tvshow's own
MDI desktop (input goes in, output comes back, in a `TWindow` tvshow owns) rather than popping
open a separate external window. An in-process dlopen plugin ABI was considered and rejected: it
directly undercuts `adr-multiprocess-sandboxing`'s accepted-risk rationale (that ADR accepts
single-process risk for *our own* degrade-gracefully code — loading untrusted 3rd-party code into
that same unisolated process is strictly worse, not an equivalent risk).

## Decision

3rd-party extension = any executable (any language, no ABI, no build-time coupling to tvshow) that
speaks a line-based protocol over stdin/stdout. **As implemented** (revised from the
`[window-providers]`-table sketch below — our `Config` parser is a flat `key = "value"` subset,
no `[section]` support): `config.toml` registers one per flat `window-provider-<name>` key:
```toml
window-provider-calculator = "/path/to/extensions/calculator/calculator.py"
```
Ctrl-X (or the Window menu's "Extension...") opens a `TWindow` that `fork`+`exec`s the mapped
command and wires the window's input line to the child's stdin, one line per Enter keypress. Child
writes response lines to stdout; tvshow appends each to the window's scrollback region as plain
text. Child exit (crash or quit) is reaped via `waitpid`; window shows `[extension exited]` and
stays open (scrollback preserved) rather than closing out from under the user.

**Superseded in part by `adr-extension-ui-protocol`**: the "no structured protocol in v1" call
below held only until there were two real motivating examples (calculator, calendar) wanting real
buttons instead of a text log. Plain-scrollback mode described here is now the *fallback* — any
child that doesn't opt in by printing `UI_INIT` as its first line — not the only mode. See that
ADR for the structured `BUTTON`/`TEXT`/`CLICK` protocol.

**Translator moved out entirely** (`adr-translator-native-window`): it's a native `TWindow` now,
not a window-provider — a translation form (language pickers, text field, Translate button)
needed widgets this protocol was never built to describe, and building it natively was simpler
than growing the protocol for one example.

**Autonomy note**: tvshow ships two working, dependency-free reference extensions under
`extensions/` (`calculator/`, `calendar/`) — pure-stdlib Python, nothing to install beyond a
Python 3 interpreter, no assumption that some other CLI tool (`bc`, `tvshow-calc`, etc.) is
already present on the user's `$PATH`. This principle carried over to the native Translator too:
its one unavoidable dependency (a translation API) fails with a clear error line, not silence,
when `DEEPL_API_KEY` is missing — tvshow has no package manager and can't install anything on the
user's behalf, so any dependency it can't paper over gets a loud, specific failure instead of a
window that looks broken.

## Consequences

- New `ExtensionWindow : TWindow` in `app/` managing one child process (`ExtensionProcess`, pipe
  I/O, non-blocking stdout reads) — impure by nature (I/O, process lifecycle), lives outside the
  pure pipeline stages (§3.1), same tier as `net/`.
- New `window-provider-<name>` flat keys in `Config` (Q-19), parsed into
  `vector<pair<string,string>>` — same flat-key shape as `handler-video`/`handler-audio`
  (`adr-external-handlers`), not a table (see revision note above).
- Crash-isolated by construction — a bad 3rd-party extension can't corrupt tvshow's own state or
  crash the main process, without needing the process-per-tab architecture that
  `adr-multiprocess-sandboxing` declined to build.
- No structured protocol in v1 for *this* ADR — plain lines in, plain lines out. `adr-extension-
  ui-protocol` later added an opt-in structured mode on top, without changing anything here for
  extensions that don't ask for it.
- 3rd party owns their own dependencies entirely (e.g. a calculator extension can vendor its own
  expression parser) — tvshow's `third_party/`/ADR-gate policy doesn't apply to them. The bundled
  reference extensions in `extensions/` default to zero such dependencies precisely so the feature
  is usable out of the box without asking the user to install anything first.

## Alternatives Considered

- **dlopen(.so) plugin ABI**: rejected — see Context; contradicts the just-accepted sandboxing
  risk posture, and commits tvshow to a stable C ABI it would then have to maintain forever.
- **Structured protocol (JSON-RPC-ish) from the start**: rejected for v1 — no real extension
  exists yet to validate the shape against; premature design. Plain-line protocol is the minimum
  that unblocks a calculator/calendar today. (Superseded once it wasn't premature anymore — see
  `adr-extension-ui-protocol`.)
- **Full terminal embedding (run the child inside a pty, forward raw terminal control codes)**:
  rejected — would let a 3rd-party extension draw arbitrary content outside tvshow's `CharGrid`/
  `TColorAttr` model, defeating the "tvshow owns the chrome" goal and reopening the crash-
  isolation-via-simplicity argument above (raw pty forwarding is a much larger, harder-to-bound
  surface than line-based stdin/stdout).

## References

- SPEC §20 q-extensions-api, §11, §3.1, Q-19
- `adr-multiprocess-sandboxing` (risk-acceptance rationale this decision preserves)
- `adr-external-handlers` (sibling ADR — fire-and-forget half of q-extensions-api)
- `adr-extension-ui-protocol` (structured mode layered on top of this ADR's protocol)
- `adr-translator-native-window` (translator moved off this mechanism entirely)
