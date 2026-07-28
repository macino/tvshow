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
speaks a line-based protocol over stdin/stdout. `config.toml` registers one under
`[window-providers]` (`calculator = "tvshow-calc"`, `translate = "tvshow-trans"`); a menu entry or
shortcut opens a `TWindow` that `fork`+`exec`s the mapped command and wires the window's input line
to the child's stdin, one line per Enter keypress. Child writes response lines to stdout; tvshow
appends each to the window's scrollback region as plain text (optional minimal inline markup,
e.g. a `\x01...\x01` bold-span wrapper, deferred to implementation — plain text is the v1
baseline). Child exit (crash or quit) is reaped via `waitpid`; window shows `[extension exited]`
and stays open (scrollback preserved) rather than closing out from under the user.

## Consequences

- New `ExtensionWindow : TWindow` in `ui/` managing one child process + its pipes — impure by
  nature (I/O, process lifecycle), lives outside the pure pipeline stages (§3.1), same tier as
  `net/`.
- New `[window-providers]` config table (Q-19), same shape as `[handlers]` (`adr-external-
  handlers`) — both are string-keyed command templates, worth sharing a `Config` parsing helper.
- Crash-isolated by construction — a bad 3rd-party extension can't corrupt tvshow's own state or
  crash the main process, without needing the process-per-tab architecture that
  `adr-multiprocess-sandboxing` declined to build.
- No structured protocol (no JSON, no schema) in v1 — plain lines in, plain lines out. A richer
  protocol (structured styling, multi-pane layout) is a natural follow-up once a real 3rd-party
  extension exists to motivate the shape, not designed speculatively now.
- 3rd party owns their own dependencies entirely (e.g. a calculator extension can vendor its own
  expression parser) — tvshow's `third_party/`/ADR-gate policy doesn't apply to them.

## Alternatives Considered

- **dlopen(.so) plugin ABI**: rejected — see Context; contradicts the just-accepted sandboxing
  risk posture, and commits tvshow to a stable C ABI it would then have to maintain forever.
- **Structured protocol (JSON-RPC-ish) from the start**: rejected for v1 — no real extension
  exists yet to validate the shape against; premature design. Plain-line protocol is the minimum
  that unblocks a calculator/translator today.
- **Full terminal embedding (run the child inside a pty, forward raw terminal control codes)**:
  rejected — would let a 3rd-party extension draw arbitrary content outside tvshow's `CharGrid`/
  `TColorAttr` model, defeating the "tvshow owns the chrome" goal and reopening the crash-
  isolation-via-simplicity argument above (raw pty forwarding is a much larger, harder-to-bound
  surface than line-based stdin/stdout).

## References

- SPEC §20 q-extensions-api, §11, §3.1, Q-19
- `adr-multiprocess-sandboxing` (risk-acceptance rationale this decision preserves)
- `adr-external-handlers` (sibling ADR — fire-and-forget half of q-extensions-api)
