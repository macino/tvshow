# tvshow extensions (window-provider examples)

Three example `window-provider-*` extensions (adr-external-window-provider,
`SPEC.md` §20 q-extensions-api). Each is a standalone script speaking a
simple line-based protocol over stdin/stdout — no build step, no ABI, no
dependency on tvshow's own build system. Any language works; these happen to
be Python (stdlib only, no `pip install` needed).

## Protocol

- tvshow spawns the configured command once, when you open the extension window.
- Each Enter in the window's input line is written to the child's **stdin**, one line.
- Anything the child writes to **stdout** is appended to the window's scrollback.
- **Flush after every write** (`flush=True` in Python, or unbuffered stdio in
  other languages) — tvshow polls the pipe non-blockingly on every idle tick;
  buffered output just sits invisible until the buffer happens to fill.
- The child exiting is not an error — the window shows `[extension exited]`
  and stays open with its scrollback intact.

## Setup

1. Make sure the script is executable: `chmod +x extensions/*/*.py` (already
   done in this repo).
2. Add one `window-provider-<name>` line per extension to
   `~/.config/tvshow/config.toml`:

   ```toml
   window-provider-calculator = "/absolute/path/to/tvshow/extensions/calculator/calculator.py"
   window-provider-calendar   = "/absolute/path/to/tvshow/extensions/calendar/calendar_provider.py"
   window-provider-translate  = "/absolute/path/to/tvshow/extensions/translator/translator.py"
   ```

   Use absolute paths — tvshow's config loader doesn't expand `~` or resolve
   relative paths, and the command is split on whitespace only (no quoting),
   so avoid spaces in the path.

3. For the translator, export your DeepL API key **before** launching
   tvshow (never put it in config.toml — that file is plaintext on disk):

   ```sh
   export DEEPL_API_KEY="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx:fx"
   ./tvshow
   ```

4. In tvshow, press **Ctrl-X** ("Extension..."). With one provider configured
   it opens directly; with more than one, pick from the list.

## What's here

| Extension | Command examples | Needs |
|-----------|-------------------|-------|
| `calculator/calculator.py` | `2 * (3 + 4)`, `sqrt(16)` | nothing (stdlib) |
| `calendar/calendar_provider.py` | `today`, `2026-07`, `2026-07-28` | nothing (stdlib) |
| `translator/translator.py` | `Hello there`, `CS: Hello there`, `EN>CS: Hello there` | `DEEPL_API_KEY` env var |

## Writing your own

Copy `calculator/calculator.py` as a starting skeleton — the `for line in
sys.stdin: ... print(..., flush=True)` loop at the bottom is the entire
protocol. Two rules to not violate:

- **Never `eval()` the input line.** It's coming from something a user can
  type — `calculator.py` shows the AST-walk pattern for "safe expression
  evaluator" instead.
- **Always flush.** Silent, unflushed stdout looks exactly like a hung
  extension from tvshow's side.
