# tvshow extensions

**New to extensions?** Start with [`docs/extension-guide.md`](../docs/extension-guide.md) — a
decision guide for which of the mechanisms below to use, plus quickstarts for each. This file is
the detailed protocol/API reference the guide links back to.

**Note:** tvshow itself now has a native Calculator, Calendar, Puzzle, Char Chart, and Event
Viewer built in (Tools menu) — see `adr-native-demo-windows.md`. The `calculator/`/`calendar/`
scripts here are **not** what those use; they're kept as a working reference implementation of
the structured window-provider UI protocol, for anyone writing a real 3rd-party extension in a
language other than C++. If you just want a calculator in tvshow, use the Tools menu, not
`window-provider-calculator`.

Two window-provider examples (`calculator/`, `calendar/`) plus the backend
script for the native Translator window (`translator/`) — see
[`docs/decisions/adr-external-window-provider.md`](../docs/decisions/adr-external-window-provider.md),
[`adr-extension-ui-protocol.md`](../docs/decisions/adr-extension-ui-protocol.md),
[`adr-translator-native-window.md`](../docs/decisions/adr-translator-native-window.md), and
[`adr-native-demo-windows.md`](../docs/decisions/adr-native-demo-windows.md).

## Four extension modes

### 1. Structured UI mode (`calculator/`, `calendar/`) — real buttons

The child's **first** stdout line is exactly `UI_INIT`. From then on it
describes widgets instead of printing free text; tvshow renders real
`TButton`s and text areas and reports clicks back over stdin. No input line,
no scrollback — everything is buttons.

```
UI_INIT
TITLE value="Calculator"
BUTTON id=1 x=0 y=2 w=5 label="7"
TEXT   id=100 x=0 y=0 w=24 value="0"
```
tvshow → child, on a press:
```
CLICK id=1
```
Full grammar (`client/include/tvshow/app/extension_ui_protocol.hpp`):

| Command | Fields | Effect |
|---|---|---|
| `UI_INIT` | — | first line only; opts into this mode |
| `BUTTON`  | `id x y w label="..."` | create/update a button (same `id` = update in place) |
| `TEXT`    | `id x y w [h=1] value="..."` | create/update a read-only text area; `\n` in `value` = newline, needs matching `h` |
| `CLEAR`   | — | destroys all tracked buttons/text areas |
| `TITLE`   | `value="..."` | sets the window title |

Values are double-quoted; `\"`, `\n`, `\\` are the only recognized escapes.
Widget positions are absolute (window content top-left = `0,0`) and don't
reflow on resize — v1 limitation, documented in the ADR.

### 2. Plain-scrollback mode (default/fallback) — free text

Any child that *doesn't* print `UI_INIT` first keeps the original protocol:
one input line feeds stdin one line per Enter, whatever the child writes to
stdout is appended to a scrollback log. This is what every extension used
before `adr-extension-ui-protocol` existed, and it's still there for
anything that's naturally a REPL rather than a form (a debugger, a shell,
etc.) — nothing you write has to adopt the structured mode.

## Common rules, either mode

- tvshow spawns the configured command once, when you open the window.
- **Flush after every write** (`flush=True` in Python, or unbuffered stdio
  elsewhere) — tvshow polls the pipe non-blockingly on every idle tick;
  buffered output just sits invisible until the buffer happens to fill.
- The child exiting is not an error. In scrollback mode the window shows
  `[extension exited]` and stays open with its log intact.

## Setup

1. Scripts are executable: `chmod +x extensions/*/*.py` (already done).
2. Add one `window-provider-<name>` line per **structured/scrollback**
   extension to `~/.config/tvshow/config.toml` (absolute paths — the loader
   doesn't expand `~`, and the command is split on whitespace only, so avoid
   spaces in the path):

   ```toml
   window-provider-calculator = "/absolute/path/to/tvshow/extensions/calculator/calculator.py"
   window-provider-calendar   = "/absolute/path/to/tvshow/extensions/calendar/calendar_provider.py"
   ```

3. The **Translator** is different — it's a native window, not a
   window-provider. Point tvshow at the backend script with a single key
   instead:

   ```toml
   translator-script = "/absolute/path/to/tvshow/extensions/translator/translator.py"
   ```

   Export your DeepL API key before launching tvshow — never in
   config.toml, that file is plaintext on disk:

   ```sh
   export DEEPL_API_KEY="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx:fx"
   ./tvshow
   ```

4. In tvshow: **Ctrl-X** ("Extension...") opens a configured window-provider
   (direct if only one, a picker if several). **Tools → Translate...** opens
   the native Translator window.

### 3. HTTP extension server (`calculator/server.py`) — plain HTML

A third, separate mechanism (`adr-extension-server`): tvshow runs an internal HTTP server
(`127.0.0.1:<extension-server-port>`, default `8765`, started lazily on first use) that routes
`/extensions/<name>/...` to a per-request spawn of an `extension.toml`-declared `entry` command.
The extension is just a CGI-lite script that reads a request off stdin and writes an HTTP-shaped
response to stdout — no widget vocabulary, the response is plain HTML rendered by tvshow's normal
browser pipeline, opened as an ordinary tab (`Tools → Extension: Calculator (HTTP)...`).

```
extension.toml:
    name = "calculator"
    entry = "python3 server.py"
    # install = "pip install -r requirements.txt"   (optional, run once)

tvshow -> extension (stdin, then closed):
    METHOD <GET|POST>
    PATH <path>
    QUERY <url-encoded query string>
    <blank line>
    <raw request body>

extension -> tvshow (stdout):
    Status: <code>
    Header-Name: value
    <blank line>
    <raw response body>
```

This is **additive**, not a replacement for the structured-UI/scrollback modes above — see
`adr-extension-server` for when to reach for which.

### 4. Client-side scripting (`<script type="text/lua">`) — instant, no round-trip

An HTTP extension page (mode 3 above) can go further: instead of every click re-fetching through
the CGI gateway, embed a sandboxed Lua script that updates the page in place. tvshow runs one Lua
VM per loaded page; a `<button onclick="handlerName" value="...">` calls a same-named zero-arg Lua
function instead of submitting a form, which can read/write existing elements' text by `id`:

```html
<span id="display">0</span>
<button onclick="press" value="7">7</button>
<script type="text/lua">
function press() tv.set_text("display", "7") end
</script>
```

`tv.set_text(id, str)` / `tv.get_text(id)` is the entire API surface — no filesystem, network, or
process access is reachable from script code (not just disallowed by policy: `io`/`os`/`package`/
`debug` are never linked into the sandboxed VM, and `load`/`loadstring`/`dofile`/`require` are
stripped out of `base`), and a runaway loop is bounded by an instruction budget rather than being
able to hang tvshow. `extensions/calculator/server.py` is the worked example — see
`adr-sandboxed-scripting` for the full design (why Lua, the sandboxing guarantees, what's
deliberately deferred).

No `<form>` needed for a scripted button — `onclick` is checked before falling back to normal
form-submit behavior, and works with or without a `<form>` ancestor.

## What's here

| Path | Mode | Needs |
|------|------|-------|
| `calculator/calculator.py` | structured UI (buttons) | nothing (stdlib) |
| `calculator/server.py` | HTTP extension server + client-side Lua scripting | nothing (stdlib) |
| `calendar/calendar_provider.py` | structured UI (buttons) | nothing (stdlib) |
| `translator/translator.py` | native-window backend (one-shot, not a window-provider) | `DEEPL_API_KEY` env var |

## Writing your own

**Want a REPL-style tool** (scrollback mode): copy `translator/translator.py`
minus the DeepL bits — the `for line in sys.stdin: ... print(..., flush=True)`
loop is the entire protocol.

**Want real buttons** (structured UI mode): copy `calculator/calculator.py`.
Print `UI_INIT` first, describe your `BUTTON`/`TEXT` layout, then loop
reading `CLICK id=N` lines and re-emitting `TEXT` to update the display.

Two rules not to violate, either mode:

- **Never `eval()` the input line.** It's coming from something a user can
  type or click — `calculator.py` shows the AST-walk pattern for "safe
  expression evaluator" instead.
- **Always flush.** Silent, unflushed stdout looks exactly like a hung
  extension from tvshow's side.
