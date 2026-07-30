# Writing a tvshow extension

This is the "how do I build one" guide. For exact protocol grammar and worked examples, see
[`extensions/README.md`](../extensions/README.md); for the design rationale behind each
mechanism, see the ADRs linked throughout.

## Three mechanisms, pick one

tvshow has no in-process plugin ABI (`q-extensions-api` — rejected on purpose, see
`adr-multiprocess-sandboxing`). Every extension is a separate process or a sandboxed script;
nothing you write runs with tvshow's own privileges beyond what you explicitly wire up.

| You want... | Use | Why |
|---|---|---|
| A REPL-ish tool (shell, debugger, chat) | **Window-provider, scrollback mode** | One input line + a scrollback log is the natural UI for line-oriented tools; nothing to build. |
| Real buttons/text areas, low-latency, any language | **Window-provider, structured UI mode** | tvshow renders actual `TButton`/text widgets; your process just describes them and reacts to clicks. No HTML, no browser pipeline involved. |
| A page-shaped tool (forms, links, tables) served like a mini web app | **HTTP extension server** | Your extension is an ordinary web page, rendered by tvshow's own HTML/CSS/forms pipeline — reuse what's already there instead of hand-describing widgets. |
| Instant UI feedback with no round-trip (a calculator, a live counter) | **HTTP extension server + Lua scripting** | An `onclick` handler runs in-process in a sandboxed Lua VM and mutates the page directly — no HTTP request per interaction. |

All three are **additive** — nothing here replaces another. Pick per-extension based on what it
actually needs, not project-wide.

## 1. Window-provider (scrollback or structured UI)

Your program is a subprocess tvshow launches once, talking line-oriented text over stdin/stdout.
Two modes, chosen by what your first line of output is:

- **Say nothing special** → scrollback mode. tvshow shows an input line + a log; your program
  reads a line, writes a line, repeat. This is the entire protocol — see
  `extensions/translator/translator.py` for the ~15-line pattern.
- **Print `UI_INIT` first** → structured UI mode. From then on your program describes `BUTTON`/
  `TEXT` widgets instead of free text, and reads `CLICK id=N` lines back. Full grammar and a
  worked example (`extensions/calculator/calculator.py`) in
  [`extensions/README.md`](../extensions/README.md) (§1, "Structured UI mode").

Configure it:
```toml
# ~/.config/tvshow/config.toml — absolute path, command split on whitespace only
window-provider-mytool = "/absolute/path/to/mytool.py"
```
Reachable via **Ctrl-X** in tvshow (direct if it's the only one configured, a picker if several).

Design rules that apply either mode:
- **Never `eval()` the input line.** It's user-controlled. `calculator.py` shows the AST-walk
  pattern for a safe expression evaluator instead of a real `eval`.
- **Flush after every write.** tvshow polls the pipe non-blockingly; buffered stdout just sits
  invisible until the buffer fills.
- Your process exiting isn't an error — scrollback mode shows `[extension exited]` and leaves the
  log intact.

See `adr-external-window-provider` and `adr-extension-ui-protocol` for the full design.

## 2. HTTP extension server

Your program is a CGI-lite script: tvshow spawns it **fresh, once per HTTP request** (no
persistent process, same crash-isolation property as the window-provider model), feeds it the
request over stdin, and renders whatever HTML it writes to stdout as a normal browser tab.

**Directory layout** (bundled extensions live under `extensions/<name>/`; this repo doesn't yet
support a user-local extensions directory — see the "Not built yet" section below):
```
extensions/mytool/
├── extension.toml   # name, entry, optional install
└── server.py        # (or any language — entry is just a shell command)
```

`extension.toml`:
```toml
name = "mytool"
entry = "python3 server.py"
# install = "pip install -r requirements.txt"   # optional, run once, on first request
```

The protocol (`extensions/README.md` has the exact field grammar):
```
tvshow -> extension (stdin, then closed):
    METHOD <GET|POST>
    PATH <path>
    QUERY <url-encoded query string>
    <blank line>
    <raw request body>

extension -> tvshow (stdout):
    Status: <code>
    Content-Type: text/html
    <blank line>
    <raw response body>
```

A minimal example — read the request, ignore it, always answer the same page:
```python
#!/usr/bin/env python3
import sys

sys.stdin.read()  # drain the request
page = "<!doctype html><html><body><h1>Hello from mytool</h1></body></html>"
sys.stdout.write(f"Status: 200\nContent-Type: text/html\n\n{page}")
sys.stdout.flush()
```

tvshow serves it at `http://127.0.0.1:<extension-server-port>/extensions/mytool/` (port defaults
to `8765`, configurable via `extension-server-port` in `config.toml`, server starts lazily on
first use — nothing listens until something actually opens an extension URL). There's no generic
picker UI yet; wire a menu command the way `cmOpenHttpCalculator` does (`application.cpp`), or
just navigate there directly with Ctrl-L.

**Write ordinary HTML/CSS your page needs** — it goes through tvshow's real HTML/CSS/forms
pipeline (SPEC §6/§7's subset), same as any other page. Two things worth knowing before you reach
for `<form>`:
- `<button>`/submit-kind `<input>` never report *which* control was clicked
  (`layout::collect_control()` skips `FormControlKind::Submit` on purpose) — a shared `<form>`
  with several differently-valued submit buttons can't disambiguate them server-side. Give each
  button its own value baked into a link's `href`, or use per-button hidden fields in separate
  forms, or (better, if you want instant feedback anyway) switch to Lua scripting below.
- `<form>` is `display: block` in tvshow's UA stylesheet — a form-per-button design will stack
  one button per row instead of packing into a grid. Plain `<button>` elements need no `<form>`
  ancestor at all if you're using `onclick` (see below).

Two `<meta>` tags let your page size/color its own window (cosmetic — the window frame only, not
your page's own content background; use ordinary CSS `body { background: ...; }` for that):
```html
<meta name="tvshow-window-size" content="26x14">
<meta name="tvshow-window-color" content="gray">  <!-- gray | cyan | blue -->
```

See `adr-extension-server` for the full design and the real bugs its live testing caught (worth
reading before you design a button grid — the two points above came from there).

## 3. Client-side Lua scripting

Layer this on top of an HTTP extension page when a click should update the page **instantly**,
with no HTTP round-trip. tvshow runs one sandboxed Lua VM per loaded page (created once, at page
load — not per click, so a script's own local state persists across clicks within that page).

```html
<span id="counter">0</span>
<button onclick="bump" value="+1">+1</button>
<script type="text/lua">
local n = 0
function bump()
  n = n + 1
  tv.set_text("counter", tostring(n))
end
</script>
```

`onclick="handlerName"` on a `<button>` calls that zero-argument Lua function instead of
submitting a form — no `<form>` ancestor needed at all. Typed keys matching a button's `value`
(and Enter → `=` if a button with that value exists) also trigger it, not just mouse clicks —
your keypad responds to direct typing for free.

**The entire API surface**: `tv.set_text(id, str)` and `tv.get_text(id)`, both scoped to an
existing element's text content by its `id` attribute. That's it — deliberately. There's no node
insertion/removal (every element a script touches must already exist in your initial HTML with
its placeholder text) and no general event model (only `onclick`).

**Sandboxing is structural, not a promise**: `io`/`os`/`package`/`debug` are never linked into the
VM at all (no filesystem, no process spawn, no env access — the capability doesn't exist to
reach), and `load`/`loadstring`/`dofile`/`require` are stripped out of `base` (a script can't load
new code strings at runtime). An instruction-count budget bounds runaway loops — a `while true do
end` aborts near-instantly instead of hanging tvshow. If you want to double-check any of this
yourself rather than take the doc's word for it, `tests/unit/script/lua_engine_test.cpp` is the
regression suite that pins these guarantees down.

**No `load`/`eval` means no generic expression evaluation** — if your extension needs to compute
something from user-typed text (a calculator, say), write it as explicit Lua logic (an
accumulator, a small state machine), not `"7*8"` fed through an interpreter call. See
`extensions/calculator/server.py` for a complete worked example (a running-total accumulator, not
an AST-walking expression parser, precisely to avoid needing anything eval-shaped).

See `adr-sandboxed-scripting` for the full design, including what's deliberately out of scope for
now (persistent state across page navigations, timers, DOM insertion/removal).

## Testing your extension

Don't reach for a terminal first:

- **`tvshow-capture <url> [cols] [rows]`** — headless, renders a URL to plain text + prints
  focused-link/color-sample info. Fast iteration loop for layout/CSS questions (this is how the
  `<form>`-vs-`<button>` and inline-vs-block-background bugs referenced above were actually
  found — screenshotting a real terminal is much slower for that kind of A/B check).
- For the real thing — actual keypress/mouse behavior, window chrome, colors as tvision paints
  them (not just as `CharGrid` computed them) — a live capture is still the only way: run tvshow
  in a real terminal and screenshot it. `scripts/take-screenshots.sh` has the working pattern
  (kitty + `xwininfo` + ImageMagick `import`) if you're doing this outside an interactive session.
- `ctest --test-dir build` runs the pure-pipeline unit tests — useful if your change touches
  tvshow's own code (a new `<meta>` hint, a new sandboxed API call), not needed for an extension
  that's just consuming the existing protocols.

## Config reference

| Key | Used by | Default |
|---|---|---|
| `window-provider-<name>` | window-provider (Ctrl-X) | — (unset = not offered) |
| `translator-script` | the native Translator window (not a general mechanism — see `adr-translator-native-window`) | — |
| `extension-server-port` | HTTP extension server | `8765` |

## Not built yet — flagged, not hidden

- No user-local extensions directory (`~/.local/share/tvshow/extensions/`) — the HTTP extension
  server currently only scans this repo's own `extensions/` directory. Installing a third-party
  HTTP extension today means adding it here, not dropping a directory somewhere else.
- No generic "browse installed HTTP extensions" picker — each one needs its own menu wiring
  (`cmOpenHttpCalculator` is the only example) or a manually-typed URL.
- No `addEventListener`-style event model for Lua scripting — `onclick` only.
- No persistent Lua state across page navigations, no timers/intervals.

These are real gaps, not oversights — `adr-extension-server` and `adr-sandboxed-scripting` record
them as deliberate v1 scope cuts, worth building once a second real extension exists to design
the generalization against.
