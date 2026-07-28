# tvshow — User Guide

tvshow is a terminal web browser. It fetches pages over HTTP, renders HTML+CSS into character cells using box-drawing glyphs and truecolor, and presents them in a TurboVision MDI desktop.

---

## Installation

### Build from source

Requirements: C++20 compiler (GCC ≥ 12 or Clang ≥ 14), CMake ≥ 3.20, Ninja, ncurses development headers.

```sh
git clone https://gitlab.com/tomas.macik/tvshow.git
cd tvshow
cmake --preset debug
cmake --build --preset debug
```

The client binary is at `build/debug/client/tvshow`.

### Optional: demo server

```sh
cmake --build --preset debug --target tvshow-srv
./build/debug/server/tvshow-srv --port 8080
```

---

## Starting tvshow

```sh
# Open a URL directly
./tvshow http://localhost:8080/

# Start with no URL (blank tab)
./tvshow
```

The application opens a TurboVision desktop with a menu bar and status line. Each browser tab is an MDI child window.

![tvshow startup — demo index page](screenshots/startup.png)

---

## Navigation

### Address bar

Press **Ctrl-L** to open the address bar. Type a URL and press Enter to navigate. Press Esc to cancel.

Accepted URL schemes: `http://` and `https://`.

![Address bar dialog (Ctrl-L)](screenshots/address-bar.png)

### Links

Tab and Shift-Tab cycle focus through links on the page. The focused link is highlighted. Press **Enter** to follow it.

Mouse clicks on a link also navigate.

### Back and Forward

| Action | Key |
|--------|-----|
| Back | Alt-← |
| Forward | Alt-→ |

Back/Forward navigate the per-tab history stack.

### Reload

Press **Ctrl-R** or **F5** to reload the current page. A spinner animates while loading.

---

## Tabs

| Action | Key |
|--------|-----|
| New tab | Ctrl-T |
| Close tab | Ctrl-W |
| Open URL in current tab | Ctrl-L |

Each tab is an independent MDI window with its own history stack. Use the **Window** menu to switch between open tabs, or use standard TurboVision window controls (Alt-F5 to zoom, Alt-F7 to move, Alt-F8 to resize).

![Two tabs open — layout and typography pages](screenshots/tabs.png)

---

## Scrolling

| Action | Key |
|--------|-----|
| Scroll down one row | ↓ |
| Scroll up one row | ↑ |
| Page down | PgDn |
| Page up | PgUp |
| Top of page | Home |
| Bottom of page | End |

The vertical scrollbar on the right edge of each tab reflects the current scroll position.

Fragment links (`#section-id`) scroll directly to the target anchor.

![Typography page scrolled to show mid-page content](screenshots/scroll.png)

---

## Forms

Tab/Shift-Tab moves focus between form fields. Supported controls:

| Control | Appearance | Interaction |
|---------|------------|-------------|
| Text input | `[ __________ ]` | Type to edit |
| Password | `[ ●●●●●●●●●● ]` | Characters hidden |
| Textarea | Multi-line bordered box | Type to edit, scrolls on overflow |
| Checkbox | `[ ]` / `[x]` | Space to toggle |
| Radio | `( )` / `(•)` | Space to select |
| Select | `[ Option ▾ ]` | Enter opens dropdown |
| Button / Submit | `[ Label ]` | Enter to activate |

Press **Enter** on a submit button (or an `input type="submit"`) to submit the form. GET forms append a query string to the action URL; POST forms send `application/x-www-form-urlencoded`.

![Forms demo page](screenshots/forms.png)

---

## Hover

`:hover` CSS styles are supported. Move the mouse over elements to see hover effects (background changes, color changes, border highlights). The browser re-resolves styles and repaints on each hover change.

## Images

An `<img>` tag defaults to showing `[alt text]` in the space reserved by the `width`/`height` attributes. Two renderers can decode and show actual image content instead, selected via `image-renderer = "alt" | "braille" | "ascii"` in `config.toml` (or `--image-renderer=...`):

- **braille** — Unicode braille patterns (U+2800..U+28FF), 2×4 sub-cell resolution.
- **ascii** — classic ASCII-art, one pixel-block average per cell on a 10-level luminance ramp (` .:-=+*#%@`). Coarser than braille but works on any terminal/font.

---

## Text selection

Click and drag with the mouse over rendered text to select it (reverse-video highlight). Release to finish. **Ctrl-C** copies the selection to the terminal clipboard (via OSC 52); with nothing selected, Ctrl-C falls back to copying the focused link's URL instead. Selection is cleared on scroll, resize, or navigating away. Keyboard-only selection (Shift+arrows) is not supported — mouse only.

## Save Link As

With a link or media placeholder focused, press **Ctrl-S** to fetch it and save the response to disk. A file-save dialog opens pre-filled with the last directory you saved to (remembered in `config.toml`).

## Network log (dev tools)

Press **Ctrl-N** to open a read-only dialog listing recent HTTP requests for the current session — method, status, byte count, elapsed time, URL — newest first, capped at the last 50. Complements the Ctrl-D box-outline overlay; there is no JS console (tvshow doesn't run JavaScript).

## Content blocklist

`~/.config/tvshow/blocklist` can list two kinds of rule, one per line:

```
block: http://ads.example.com/*
hide: .ad-banner
```

`block:` lines (URL glob patterns) stop the request before it's issued — the response renders as an empty page instead. `hide:` lines are CSS selectors that get an implicit `display: none`, lower priority than the page's own CSS so authors can still override it. No file present = no blocking, same as today.

## Extensions

Press **Ctrl-X** ("Extension...") to open a 3rd-party extension in its own embedded window — a calculator, translator, or anything else that speaks plain text over stdin/stdout. Configure one or more in `config.toml`:

```toml
window-provider-calculator = "/path/to/calculator.py"
window-provider-translate  = "/path/to/translator.py"
```

With one provider configured, Ctrl-X opens it directly; with several, it shows a picker first. See [`extensions/README.md`](../extensions/README.md) for working examples (calculator, calendar, DeepL translator) and the protocol extensions must follow.

Related, fire-and-forget (no embedded window): `handler-video`/`handler-audio` in `config.toml` spawn an external player (e.g. `mpv %s`) when you press Enter on a video/audio link, instead of trying to navigate to it.

---

## Menu reference

| Menu | Items |
|------|-------|
| **≡ tvshow** | About, Quit (Alt-X) |
| **File** | New Tab (Ctrl-T), Open URL (Ctrl-L), Close Tab (Ctrl-W) |
| **Navigate** | Back (Alt-←), Forward (Alt-→), Reload (Ctrl-R), Stop (Esc), Home |
| **View** | Toggle Address Bar, Toggle Status, Cascade, Tile |
| **Window** | List of open tabs, Extension... (Ctrl-X) |

---

## Keyboard reference

| Key | Action |
|-----|--------|
| Ctrl-L | Open address bar |
| Ctrl-T | New tab |
| Ctrl-W | Close tab |
| Ctrl-R | Reload |
| F5 | Reload |
| Alt-← | Back |
| Alt-→ | Forward |
| Esc | Stop / cancel |
| Tab | Focus next link/control |
| Shift-Tab | Focus previous link/control |
| Enter | Follow link / activate / submit |
| Space | Toggle checkbox/radio |
| ↑ / ↓ | Scroll one row |
| PgUp / PgDn | Scroll one page |
| Home / End | Top / bottom of page |
| Ctrl-D | Toggle debug overlay (box outlines + dimensions) |
| Ctrl-F | Find in page |
| Ctrl-B | Bookmarks |
| Ctrl-C | Copy selection, else focused link URL |
| Ctrl-S | Save Link As (focused link/media) |
| Ctrl-N | Network log |
| Ctrl-X | Open extension window |
| Mouse drag | Select text |
| Alt-X | Quit |

---

## Logs and diagnostics

Logs are written to `~/.cache/tvshow/log`. Use `--log-level debug` for verbose output:

```sh
./tvshow --log-level debug http://localhost:8080/
```

---

## Scroll indicator

When a page is taller than the viewport, the window title shows the current scroll position as a percentage (e.g. `[42%]`).

## Debug overlay

Press **Ctrl-D** to toggle box outlines drawn in magenta over the rendered page. Each box shows its dimensions (WxH in cells) at the top-right corner.

---

## Known limitations

- No JavaScript.
- Cookies with `Max-Age`/`Expires` persist across restarts (`~/.config/tvshow/cookies`); session cookies (no expiry) don't, matching browser behavior. No `Secure`/`SameSite` enforcement.
- Images default to `[alt]` text (braille/ascii renderers require pre-fetched image data).
- Text selection is mouse-only; no keyboard (Shift+arrow) range-select.
- `:hover` applies to the directly hovered element only (not ancestors).
- No JS console in the network log — there's nothing to console.log from.
- Extensions (Ctrl-X) and video/audio handlers run as plain child processes: no sandboxing beyond normal OS process isolation.
