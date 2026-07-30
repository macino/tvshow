# adr-extension-server — internal HTTP gateway for extensions

- **Status:** accepted
- **Date:** 2026-07-30
- **Touches SPEC:** §20 q-extensions-api, §16 (out of scope until now — tvshow embeds an HTTP
  server, not just a client)

## Context

tvshow is fundamentally an HTTP browser. The existing extension mechanisms
(`adr-external-window-provider`/`adr-extension-ui-protocol`) instead hand-roll a bespoke
stdin/stdout widget protocol per extension, reimplementing a slice of "render a form" that the
project's own HTML+CSS+forms pipeline already does, tested, end to end. That's backwards for
anything an extension author would naturally write as a web form: a calculator, a settings panel,
a small dashboard.

The ask: let an extension be plain HTML, served by a small server tvshow itself runs, and browsed
like any other URL. No new client-side protocol, no new widget vocabulary — reuse the pipeline
that already exists.

## Decision

An internal HTTP server (`net::ExtensionServer`), embedded in the `tvshow` client process:

- **Transport**: `cpp-httplib` in server mode (already vendored for `tvshow-srv`; the client
  already links it as an HTTP *client* for `net::CppHttpClient` — this adds the server side of the
  same dependency, no new one), bound to `127.0.0.1:<port>` only — never `0.0.0.0`, this is a
  loopback-only gateway to locally-installed extensions, not a public server. Runs on a background
  `std::thread`, started **lazily** on first use (`Application::ensure_extension_server()`), not
  at every launch — most sessions won't touch an HTTP extension at all.
- **Config**: `extension-server-port` (default `8765`). Not configurable per-extension; one server,
  one port, all installed extensions routed under it.
- **Extension layout**: a directory with an `extension.toml` manifest —
  `name`/`entry`/`install` (install optional). `entry` is a shell command run **per request**,
  with cwd set to the extension's own directory (so `entry = "python3 server.py"` resolves
  relative to itself, not tvshow's cwd). Bundled extensions live under `extensions/` (this repo);
  the server scans whatever directories it's constructed with — user-installed extensions under
  e.g. `~/.local/share/tvshow/extensions/` would be a second entry in that list, not implemented
  in this pass (single bundled-`extensions/`-dir scaffold only, see Consequences).
- **Install**: if `install` is non-empty, run it once (`std::system`, blocking, on the server's own
  thread — never the TUI thread) the first time the extension is requested; success is recorded in
  a marker file under `$XDG_STATE_HOME/tvshow/extensions-installed/` (keyed by a hash of the
  extension's directory, so bundled and user extensions sharing a name can't collide) so it isn't
  re-run on every subsequent tvshow launch. A failed install returns `503` and is retried on the
  next request (no permanent "broken" state).
- **Request gateway — CGI-style, per-request spawn, not a persistent process**: every HTTP request
  to `/extensions/<name>/...` spawns a fresh child via the existing `app::ExtensionProcess`
  (fork+exec+pipe, the same proven plumbing `ExtensionWindow`/`TranslatorWindow` already use for
  window-provider extensions). tvshow writes a small CGI-lite request to the child's stdin —
  `METHOD <verb>\nPATH <path>\nQUERY <qs>\n\n<body>` — then closes stdin (signals EOF, so a script
  reading with `sys.stdin.read()` doesn't block forever) and reads stdout to completion, parsed as
  `Status: <code>\nHeader: value\n...\n\n<body>` (classic CGI response shape; `Status` optional,
  defaults to 200). No persistent per-extension process, no new process-supervision model — the
  explicit tradeoff is a process-start per request instead of request latency, matching this ADR's
  "reuse what's proven, don't build new infrastructure" stance. `ExtensionProcess` gained two small
  methods for this (`write()` for raw un-line-terminated writes, `close_stdin()` for EOF signalling)
  — both additive, its existing non-blocking-read-loop callers (`TranslatorWindow::poll()`,
  `ExtensionWindow`) are unaffected.
- **Client-side wiring**: none, beyond starting the server and calling the existing `open_url()`.
  tvshow navigates to `http://127.0.0.1:<port>/extensions/<name>/` exactly like any other URL —
  a normal `BrowserWindow` tab, full HTML/CSS/forms support, no special-cased extension window
  class. This is the entire point of the design: the browser pipeline *is* the extension UI.

**Proof of concept**: `extensions/calculator/server.py` — the same calculator, re-hosted as HTML —
alongside (not replacing) the existing window-provider `calculator.py` in the same directory.
`extensions/calculator/extension.toml` is the manifest (`name = "calculator"`,
`entry = "python3 server.py"`, no `install` — pure stdlib). Reachable from Tools → "Extension:
Calculator (HTTP)..." — lazily starts the server, then `open_url()`s the extension.

The keypad is plain `<a href="...?expr=...&key=...">[C]</a>` links, one per key, the key and
running expression both baked into the href as a GET query string — **not** `<form>`/`<button>`,
after two rounds of live testing (screenshot + `tvshow-capture`) caught real limits in this
browser's form model that a real extension author would hit too, worth recording:

1. `layout::collect_control()` (`form_data.cpp`) explicitly skips `FormControlKind::Submit` — a
   clicked submit button's own `name`/`value` is **never** sent, only the form's other fields.
   A single `<form>` with 20 differently-valued submit buttons (the first design tried) can't be
   disambiguated server-side at all: every click submits the same empty-of-`key` request.
2. Fixing that by giving each button its own `<form>` (key as a hidden field instead) hit a second
   wall: this project's UA stylesheet sets `form { display: block; }` — every `<form>` starts its
   own line, so 20 one-button forms rendered as 20 stacked rows instead of a packed grid, and
   didn't look anything like the native Calculator window it's standing in for.

Links sidestep both: `<a>` is inline (packs into the same grid buttons would) and Enter-navigable,
and there's no "which control fired" question to begin with — the key is just part of the URL.
The tradeoff this accepts: an extension needing an actual multi-field form (not just a keypad)
still has the two numbered limitations above to design around; not a problem this ADR's scope
fixes (changing `collect_control`'s submit-button behavior is a real, separate change against
existing SPEC §6/§7-scoped form behavior, not something to slip in under an extensions ADR).

Verified live end-to-end, not just unit-tested: `tvshow-capture` (headless, `client/src/
capture_main.cpp`) against the running server confirmed the 20 links pack into the same
row-wrapped grid the buttons used to, and a real click (kitty-driven keyboard nav + Enter)
round-tripped through the actual subprocess spawn — `7`, `*`, `8`, `=` in sequence correctly
landed on `56`.

**Superseded, same session**: this links-based design was the right fix for the layout bug it
solved (`<form>`'s `display:block`), but every click was still a full page reload through the CGI
gateway — correct, but not what a calculator should feel like. `adr-sandboxed-scripting`'s "Round
two" section replaced this with an `onclick`-driven, client-side-scripted version once the user
asked for real Lua scripting directly — `extensions/calculator/server.py` now serves a single
static page a sandboxed Lua VM drives, no per-click round-trip. The CGI gateway design in this ADR
is unchanged and still real (the calculator's one remaining request is the initial page fetch);
only the calculator's *own* interactivity design moved on.

## Consequences

- New pure module `net::extension_manifest.{hpp,cpp}` (in `tvshow_core`, unit-tested):
  `parse_extension_manifest`, `parse_cgi_response`, `build_cgi_request` — all pure string
  transforms, no I/O, matching this project's pure/impure module split (SPEC's pipeline-purity
  rule extends naturally to this gateway's request/response framing).
- New impure module `net::ExtensionServer` (`client/src/net/extension_server.cpp`), built directly
  into the `tvshow` executable (same placement as `app::ExtensionProcess`/`ExtensionWindow` —
  not `tvshow_io`, since it depends on `app::ExtensionProcess`). Not unit-tested at this layer
  (background thread + real subprocess spawn + real socket bind) — verified live via screenshot,
  same discipline `adr-native-demo-windows` established for TUI-only behavior pure tests can't
  reach.
- `util::Config` gains `extension_server_port` (default `8765`).
- `app::ExtensionProcess` gains `write()`/`close_stdin()`.
- New `cmOpenHttpCalculator` command + Tools menu entry, wired through
  `Application::ensure_extension_server()`.
- **Scope cuts, flagged not silently dropped** (see the session's staged plan): only one bundled
  extensions directory is scanned (no user-installed-extensions directory yet); no extension
  discovery/picker UI (one hardcoded menu entry, not a generic "browse installed extensions" flow);
  no uninstall; the install-state marker has no invalidation (a changed `install` command doesn't
  re-run until the marker file is deleted by hand). All reasonable follow-ups once a second real
  extension exists to design against — building that generality against a single proof-of-concept
  would be guessing at requirements.
- **Relationship to the window-provider mechanism**: not replaced, not deprecated. Structured
  UI mode (`adr-extension-ui-protocol`) and plain-scrollback mode remain the right fit for
  anything that isn't naturally a web page (a REPL, a live-updating log, tight low-latency
  button/keypress interaction) — an HTTP round-trip per click is a real cost this mechanism
  accepts for extensions that *are* naturally pages. `docs/decisions/adr-native-demo-windows.md`'s
  five native windows are also unaffected — this is a third, additive track for extensions that
  aren't bundled tvshow features.
- **Security posture**: loopback-only bind (never reachable off-host), same trust boundary as an
  extension author running arbitrary code already implies (a window-provider extension is just as
  capable of doing anything the user's own account can do — this doesn't create a new privilege
  tvshow itself grants). No sandboxing beyond process isolation, matching
  `adr-multiprocess-sandboxing`'s existing accepted-risk posture for this project.

## Alternatives Considered

- **Persistent per-extension process** (start once, keep alive, route requests to it like a real
  app server): rejected for this pass — real process supervision (restart on crash, health checks,
  backpressure) is meaningfully more machinery than a demo/skeleton justifies; CGI-per-request is
  simpler and reuses `ExtensionProcess` unchanged. Revisit if request latency or per-request
  process-start overhead actually matters for a real extension.
- **Route extension requests through `tvshow-srv`** (the existing demo server binary) instead of a
  server embedded in the client: rejected — `tvshow-srv` is a separate process the client doesn't
  control the lifecycle of; embedding keeps "install an extension, it just works" self-contained
  in the one binary users actually run.
- **In-process plugin ABI / dlopen**: not reconsidered here — already rejected by
  `adr-multiprocess-sandboxing`'s risk posture, same reasoning `q-extensions-api` already records.

## References

- SPEC §20 q-extensions-api
- `adr-external-window-provider`, `adr-extension-ui-protocol` (the mechanism this is additive to,
  not a replacement for)
- `adr-native-demo-windows` (unaffected native Tools-menu windows)
- `adr-multiprocess-sandboxing` (accepted risk posture this inherits)
- `extensions/calculator/{extension.toml,server.py}` (proof of concept)
