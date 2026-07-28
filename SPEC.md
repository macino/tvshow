# tvshow — Specification

A terminal "web browser" rendered with TurboVision. Server speaks real HTTP/1.1 and returns HTML+CSS; client parses, lays out into character cells, paints with `tvision` using box-drawing chars and color attributes.

> Status: **M0, M1, and M2 complete**. See §20.1/§20.2 for the feature lists.

---

## 1. Mission and Non-Goals

### 1.1 Mission
Build a hostable application stack:
- **Client (`tvshow`)** — a TurboVision-style desktop application that fetches HTTP resources, parses a defined subset of HTML/CSS, and renders them into a character cell grid with full TurboVision look-and-feel (MDI, menus, dialogs, scrollbars).
- **Server (`tvshow-srv`)** — a minimal demo HTTP server that ships sample pages and form endpoints. Used for development, demos, and integration tests.

The result must be a sound base for test-first development. Render path must be a pure function so layouts can be asserted by golden-grid tests without spawning a terminal.

### 1.2 Non-Goals (v1)
- JavaScript execution.
- CSS Grid, animations, transitions, transforms, gradients, shadows.
- Cookies, persistent sessions, auth (Open Q-9).
- Real images. Only `alt` text in v1; ASCII-art renderer is an interface stub for later.
- Web compatibility with arbitrary public sites. We render *our* HTML subset cleanly; real-world pages may degrade.

---

## 2. Locked Decisions

| Axis | Decision |
|------|----------|
| Language / runtime | C++ (C++20) with `magiblot/tvision` |
| HTML scope | Defined subset + forms (see §6) |
| Wire protocol | Real HTTP/1.1 |
| Interactivity | Links + forms + tabs (MDI windows) |
| HTML parser | `gumbo-parser` |
| HTTP client | `cpp-httplib` (header-only) |
| Test framework | `doctest` (header-only) |
| Tab UX | TurboVision MDI child windows (`TWindow` per document) |
| Layout unit mapping | px → cells: **8 px = 1 column, 16 px = 1 row** |
| Image fallback | `[alt]` text in v1; pluggable interface for ASCII-art renderer later |
| Color depth | Truecolor with 256-color and 16-color fallback, autodetected |
| CSS parser | `katana-parser` (full CSS3 grammar; we act on subset of properties) |
| Server scope | Client + minimal demo server using `cpp-httplib` |
| Build system | CMake (≥ 3.20), Ninja preferred |

---

## 3. High-Level Architecture

```
   ┌─────────┐       HTTP/1.1      ┌────────────┐
   │ tvshow  │  ─────────────────▶ │ tvshow-srv │
   │ client  │  ◀───────────────── │   demo     │
   └────┬────┘    HTML + CSS       └────────────┘
        │
        │  pipeline (pure functions, one direction):
        │
        │   bytes ─▶ DOM ─▶ CSSOM ─▶ Styled DOM ─▶ Box Tree ─▶ CharGrid
        │                                                       │
        │                                                       ▼
        │                                            tvision TDrawBuffer
        │                                                       │
        ▼                                                       ▼
    Input (keys, mouse) ◀──── focus / hit-test ◀──────── TWindow paint
```

### 3.1 Pipeline Stages (pure unless noted)
| Stage | Input | Output | Pure? |
|-------|-------|--------|-------|
| `net` | URL | bytes + headers | No (I/O — mock in tests) |
| `dom` | bytes + content-type | DOM tree (our types) | Yes |
| `css` | stylesheet text | CSSOM (rules, selectors, declarations) | Yes |
| `style` | DOM + CSSOM list (cascade order) | Styled DOM (each node has computed style) | Yes |
| `layout` | Styled DOM + Viewport (cols × rows) | Box Tree (positions in cells) | Yes |
| `render` | Box Tree | CharGrid (cell = char + `TColorAttr`) | Yes |
| `paint` | CharGrid + `TDrawBuffer` | drawn TWindow rows | No (tvision side effect) |
| `input` | key/mouse event + focus state | Action (navigate, focus, submit) | Yes |

**Test seam:** every Yes-row is unit-tested with deterministic input/output. Golden tests assert on `CharGrid` snapshots (text + attribute map) — no terminal needed.

---

## 4. Module Layout

```
tvshow/
├── CMakeLists.txt
├── SPEC.md
├── CLAUDE.md
├── README.md
├── cmake/                 # toolchain helpers, FetchContent wrappers
├── third_party/           # vendored or FetchContent'd:
│   ├── tvision/
│   ├── gumbo-parser/
│   ├── katana-parser/
│   ├── cpp-httplib/       (header-only)
│   └── doctest/           (header-only)
├── client/
│   ├── include/tvshow/    # public headers per module
│   └── src/
│       ├── main.cpp
│       ├── app/           # TApplication subclass, menu bar, status line, MDI desktop
│       ├── net/           # HttpClient interface + cpp-httplib impl
│       ├── dom/           # Gumbo → DOM adapter, our DOM types
│       ├── css/           # Katana → CSSOM adapter, selector matching, cascade, specificity
│       ├── style/         # computed-style resolver
│       ├── layout/        # block / inline / flex flow into Box tree (cell coords)
│       ├── render/        # Box tree → CharGrid (chars + TColorAttr)
│       ├── paint/         # CharGrid → TDrawBuffer (tvision-side)
│       ├── ui/            # TBrowserWindow : TWindow, TTabManager, TAddressBar dialog
│       ├── input/         # key/mouse → action mapper, link/form focus traversal
│       ├── history/       # per-tab back/forward stack
│       ├── images/        # ImageRenderer interface (v1: AltTextRenderer)
│       └── util/          # url, percent encoding, charset, logging
├── server/
│   ├── src/main.cpp       # cpp-httplib router
│   └── pages/             # sample .html / .css for demos and tests
├── tests/
│   ├── unit/              # one dir per pure module
│   ├── golden/            # *.html → *.grid expected snapshots
│   └── integration/       # spin server + run client request, compare grid
└── docs/
    └── decisions/         # ADRs for non-trivial design changes (mirrors ai-wkf)
```

---

## 5. Build, Run, Test

### 5.1 Build
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Targets:
- `tvshow` — client binary
- `tvshow-srv` — demo server binary
- `tvshow-tests` — unit + golden test runner (doctest)
- `tvshow-itests` — integration tests (server + client harness)

### 5.2 Run
```
./build/server/tvshow-srv --port 8080
./build/client/tvshow http://localhost:8080/index.html
```

### 5.3 Test
```
ctest --test-dir build --output-on-failure
```

Test discipline: tests are written **before** the implementation for any pure module. UI/paint layers are kept thin so most logic lives in test-first stages.

### 5.4 Dependency Strategy
- Header-only libs (`cpp-httplib`, `doctest`) committed under `third_party/`.
- Compiled libs (`tvision`, `gumbo-parser`, `katana-parser`) pulled via CMake `FetchContent` and pinned to a specific commit. Mirrored under `third_party/` only if upstream becomes unreliable.

---

## 6. HTML Subset (v1)

### 6.1 Document
`<!doctype html>`, `<html>`, `<head>`, `<title>`, `<meta charset>`, `<link rel="stylesheet" href>`, `<style>`, `<body>`.

### 6.2 Block flow
`div`, `p`, `h1`–`h6`, `hr`, `pre`, `blockquote`, `section`, `article`, `header`, `footer`, `nav`, `main`, `ul`, `ol`, `li`.

### 6.3 Inline
`span`, `a`, `b`, `strong`, `i`, `em`, `u`, `code`, `br`, `small`.

### 6.4 Forms
`form` (action, method GET/POST, enctype `application/x-www-form-urlencoded`), `input` (type=text|password|checkbox|radio|submit|hidden), `textarea`, `select` + `option`, `button`, `label`.

### 6.5 Images
`<img src alt width height>` — reserves `width × height` (mapped through px→cell ratio). Rendering
is pluggable (see Q-23): `AltTextRenderer` (default) renders `[alt]`; `BrailleRenderer`
(`image-renderer = "braille"`) decodes the fetched bytes and quantizes to braille glyphs.

### 6.6 Tables (v1.1)

`table`, `tr`, `td`, `th`, `thead`, `tbody`, `tfoot`, `caption`.

Layout is implemented via the existing flex engine (ADR 002):

| Element | UA display |
|---------|-----------|
| `table` | block + `border-style: solid` (outer box-drawing-char frame) |
| `thead` / `tbody` / `tfoot` | block |
| `tr` | flex; flex-direction: row |
| `td` / `th` | block; flex-grow: 1; padding-left/right: 8px |
| `caption` | block; font-weight: bold |
| `colgroup` / `col` | none |

Column widths align across rows: `compute_table_col_widths()` takes the max cell content-width
per column index across all rows and applies it uniformly (see Q-24).

`colspan`/`rowspan` supported (see Q-27). Limitations: no CSS `border-collapse`.

### 6.7 Out of subset
`script`, `canvas`, `svg` — parsed and skipped (children rendered as if their parent were `div`).
`iframe`, `video`, `audio` render as a `[Embedded: <src>]`/`[Media: <src>]` link-out instead of
being skipped (see Q-30).

---

## 7. CSS Subset (v1)

### 7.1 Sources
1. User-agent default stylesheet (built into client).
2. `<link rel="stylesheet">` external sheets (HTTP-fetched).
3. `<style>` blocks.
4. Inline `style="..."`.

Cascade order, specificity, and `!important` follow CSS spec. Katana provides selector ASTs; our matcher implements descendant, child, class, id, tag, attribute existence, `:hover` (mouse-over hit-test, style recalc on change — see Q-10), `:focus`.

### 7.2 Honored Properties
| Group | Properties |
|-------|-----------|
| Color | `color`, `background-color`, `background` (color shorthand only) |
| Text | `font-weight` (normal/bold), `font-style` (normal/italic), `text-decoration` (none/underline), `text-align` (left/right/center), `white-space` (normal/pre/nowrap) |
| Box | `width`, `height`, `min-width`, `max-width` (px, %, ch), `margin`, `padding`, `border`, `border-style`, `border-color`, `border-width`; individual longhands `border-{top,right,bottom,left}-{style,width,color}` (all 12) |
| Display | `display: block | inline | inline-block | flex | none` |
| Flex | `flex-direction`, `justify-content`, `align-items`, `gap` (single value), `flex-grow`, `flex-shrink`, `flex-basis` |
| Position | `position` (static/relative/absolute), `top`, `right`, `bottom`, `left` (px, %, ch) |
| Other | `visibility`, `overflow` (visible/hidden/scroll/auto) |

`px`, `%`, `em` (relative to parent font size = 1 cell row by default), `ch` (= 1 column), `rem` (= 1 row).

### 7.3 Unit Mapping
- `1px = 1/8 col horizontally, 1/16 row vertically`. Round half-up.
- `1ch = 1 col`, `1em = 1 row` (font size has no effect — terminals are mono-cell).
- `%` resolved against containing block's content area.

### 7.4 Ignored Properties
Anything not in §7.2 — parsed (so `style="..."` doesn't error) but not applied.

---

## 8. Color and Style Mapping

### 8.1 Color Resolution
1. Parse CSS color (named, `#rgb`, `#rrggbb`, `rgb()`, `rgba()` — alpha discarded for v1).
2. Detection at startup: `$COLORTERM` (`truecolor`/`24bit`) → truecolor; else parse `tput colors` → 256 or 16.
3. Truecolor mode: emit `TColorRGB`.
4. 256 mode: convert RGB to nearest xterm 256-cube index.
5. 16 mode: convert RGB to nearest CGA palette entry.

All conversion lives in `util::Color`, tested with table-driven cases.

### 8.2 Style Attributes (TColorAttr style flags)
| CSS | tvision flag |
|-----|-------------|
| `font-weight: bold` | `slBold` |
| `font-style: italic` | `slItalic` |
| `text-decoration: underline` | `slUnderline` |
| `text-decoration: line-through` | `slStrike` |
| `:focus` (link/form) | `slReverse` (or theme color) |

---

## 9. Borders and Box-Drawing

CSS `border-style` → box-drawing chars:

| border-style | Corners + edges |
|--------------|-----------------|
| `none` | no border, no cells reserved |
| `solid` (default) | `┌┐└┘─│` |
| `double` | `╔╗╚╝═║` |
| `dashed` | `┌┐└┘╌╎` |
| `dotted` | falls back to `solid` (terminals lack dotted) |
| `ridge` / `groove` / `inset` / `outset` | falls back to `solid` |

Border colors honored via `TColorAttr`. Border width is always 1 cell (CSS width values > 0 collapse to 1; 0 collapses to `none`).

---

## 10. Layout Engine

### 10.1 Block Formatting Context
Standard CSS block flow. Block boxes stack vertically; inline boxes flow horizontally and wrap at cell boundaries. Margin collapsing implemented for adjacent block-level siblings (vertical margins only).

### 10.2 Inline Formatting Context
Line boxes built greedily. `white-space: pre` preserves newlines and runs of spaces. `white-space: nowrap` disables wrapping — overflow handled per `overflow` property.

Inline-display form controls (`input`, `button`, `select`) participate in the inline flow as unbreakable atoms of their widget width (`form_control_size().cols`). The word-break algorithm treats each control as a single non-space word; line wrapping and text-alignment apply to it exactly as to any word token. The control's grid position is derived from the line-break result — not from a separate block-child placement pass — so label text and widget cannot overlap.

### 10.3 Flex
v1 supports `flex-direction: row | column`, `justify-content: flex-start | flex-end | center | space-between | space-around`, `align-items: flex-start | flex-end | center | stretch`, `gap`, `flex-grow`, `flex-shrink`, `flex-basis`. No wrapping (`flex-wrap: nowrap` only). Overflow truncates at the viewport edge (Q-3 resolved).

### 10.4 Viewport and Resize
Viewport = inner content area of the active `TBrowserWindow` (after subtracting frame, scrollbars, address line). On terminal resize, the active document is re-laid-out from the cached Styled DOM (no re-fetch).

---

## 11. UI Model (TurboVision)

### 11.1 Application
`TvshowApplication : TApplication` builds the standard menu bar + status line + desktop. Single instance per process.

### 11.2 Menu bar
- **≡ tvshow** — About, Quit
- **File** — New Tab (Ctrl-T), Open URL (Ctrl-L), Close Tab (Ctrl-W)
- **Navigate** — Back (Alt-←), Forward (Alt-→), Reload (Ctrl-R), Stop (Esc), Home
- **View** — Toggle Address Bar, Toggle Status, Cascade, Tile
- **Window** — list of open tabs

### 11.3 Browser Window (Tab)
`TBrowserWindow : TWindow` is one tab. Per-window state: current URL, document, history stack, scroll offset, focused element id. Multiple `TBrowserWindow` instances live in the desktop as MDI children.

New windows open with a cascade offset so they are not fully obscured: each successive window shifts +2 cols and +1 row from the previous. The offset counter wraps when it would push the window off-screen.

### 11.4 Address Bar
Default: modal `TInputDialog` opened via Ctrl-L. `--address-bar=persistent` switches to a permanent one-row `TInputLine` at the top of each browser window, shrinking the content viewport by one row. Both modes accept a URL, validate it via `Url::parse`, and trigger navigation. (Q-1 resolved.)

### 11.5 Status Line
Shows: current URL on hover, focused link target, loading/error state, key hints (`Ctrl-R Reload  Alt-← Back  Ctrl-L URL`).

---

## 12. Input Model

### 12.1 Focus
A document maintains an ordered list of *focusable* elements: links (`<a href>`) and form controls. Tab / Shift-Tab cycle. Visual focus = `slReverse` or theme-dependent attribute.

### 12.2 Activation
- Enter on focused link → navigate (push history).
- Enter on submit button or `submit`-type input → form submit.
- Space toggles checkbox/radio.
- Mouse click maps screen cell → element via hit-test on Box Tree; same actions follow.

### 12.3 Scrolling
Native `TScrollBar`. PageUp/Down, arrow keys, Home/End. `#fragment` URLs scroll to anchor (Open Q-11 for smooth/instant policy).

---

## 13. Forms

### 13.1 Submission
On submit:
1. Walk form's controls → build name/value pairs.
2. Encode per `enctype` (v1: `application/x-www-form-urlencoded` only).
3. GET → append query string to action URL, navigate.
4. POST → HTTP POST with body, navigate to response (status 2xx renders body; 3xx follows redirect; 4xx/5xx renders error page).

### 13.2 Field Rendering
- `text` / `password` — bordered single-line edit.
- `textarea` — bordered multi-line edit, scrolls on overflow.
- `checkbox` — `[ ]` / `[x]`.
- `radio` — `( )` / `(•)`.
- `select` — closed: `[ Value ▾ ]`; open: dropdown list dialog.
- `button` — `[ Label ]`, focusable.
- `hidden` — not rendered, value submitted.

---

## 14. History

Per-tab back/forward stack, classic browser semantics. Navigation pushes; Back/Forward shifts current pointer without truncating until a new navigation occurs at a non-end position (then forward stack is dropped).

---

## 15. Networking

### 15.1 Requests
`net::HttpClient` interface; default impl uses `cpp-httplib`. Tests inject a fake. Supports:
- GET / POST.
- Status follow for `301/302/303/307/308` up to 5 hops.
- `text/html`, `text/css`, `text/plain` content types in v1.
- Charset: parse from `Content-Type` (`charset=`) and meta tag; assume UTF-8 if absent. Internal string type is UTF-8 throughout (`std::string`); tvision Unicode code path used for paint.

### 15.2 Errors
Network and HTTP errors render an internal error page (UA-styled HTML). Status line shows compact reason.

### 15.3 HTTPS
`cpp-httplib` is built with `CPPHTTPLIB_OPENSSL_SUPPORT`; `https://` URLs work via `httplib::Client`'s automatic `SSLClient` selection. `util::Url` parses both schemes and defaults the port to 443 for `https://`.

---

## 16. Demo Server

`tvshow-srv` ships:
- `/` — landing page that lists all sample pages.
- `/pages/typography.html` — headings, paragraphs, inline styles.
- `/pages/layout.html` — block, flex examples.
- `/pages/forms.html` — every supported control. Submit POSTs to `/echo` which renders the parsed form.
- `/pages/colors.html` — color depth showcase.
- `/pages/borders.html` — border styles.
- `/pages/errors/404` — used by integration tests.

Used by integration tests to verify end-to-end golden grids.

---

## 17. Test Strategy (Test-First)

### 17.1 Layered
1. **Unit (per module, pure):** doctest cases live next to code. Each pure module has 100% line coverage as a goal.
2. **Golden (`tests/golden/`):** for each fixture `case.html` (+ optional `case.css`), a `case.grid` snapshot stores the expected `CharGrid` (chars on first half, attribute codes on second half, separated by a marker). Test driver re-runs the pipeline and diffs.
3. **Integration (`tests/integration/`):** spawn `tvshow-srv` on an ephemeral port, run client request via library API (no terminal), compare to golden grid.

### 17.2 Determinism
The pipeline is deterministic given (input bytes, viewport size, color depth, terminal capabilities). All non-determinism (timing, network) is behind the `net` module. Color autodetection is stubbed in tests.

### 17.3 CI
`ctest --output-on-failure` in CMake build. PR cannot merge with red tests. CI runs on GitHub Actions (`.github/workflows/ci.yml`): Ubuntu latest, clang, Ninja, cmake configure → build → ctest.

---

## 18. Logging and Diagnostics

`util::log` writes to `~/.cache/tvshow/log` by default; `--log-level` flag controls verbosity (debug / info / warn / error). Log module lives in `tvshow_io` (impure). Path `"-"` forces stderr-only output.

Ctrl-D toggles the debug overlay: box outlines (`┌─┐│└┘`) drawn in magenta over the rendered CharGrid. Applies to every box in the layout tree. Focus order display and box-dimension labels are M1 scope (Q-22, M1-debug-overlay).

---

## 19. Coding Standards

- C++20. No exceptions across module boundaries; use `std::expected`-shaped result types where errors are expected (or `tl::expected` shim if compiler lacks `<expected>`).
- Headers under `client/include/tvshow/<module>/`; one public header per module surface.
- No globals; tvision's `TApplication` is the only singleton.
- Format with `clang-format` (config in repo root). Lint with `clang-tidy` (config in repo root).
- ADRs in `docs/decisions/` for any structural change beyond this SPEC.

---

## 20. Open Questions (Carve Next)

> ID scheme: `Q-1`..`Q-30` are frozen numeric IDs — cited by code/test comments across the
> repo, never renumbered. `Q-31`+ use slug IDs (`q-<slug>`) per ai-wkf's Generated-IDs rule.
> Same split applies to `docs/decisions/` ADRs (001-004 frozen numeric, `adr-<slug>` from here on).

| # | Topic | Need |
|---|-------|------|
| Q-1 | Address bar UX | **Resolved**: modal default; `--address-bar=persistent` opt-in for permanent top bar. |
| Q-2 | Bookmarks / homepage / start page | **Resolved**: `~/.config/tvshow/bookmarks` (url\ttitle per line). Ctrl-B opens CRUD picker (Enter=open, A=add, D=delete). Loaded at startup, saved on change. |
| Q-3 | Resize behavior for flex layouts when viewport too narrow | **Resolved**: truncate — overflow clips at viewport edge; no per-container horizontal scrollbar in v1. |
| Q-4 | UA default stylesheet — how opinionated | **Resolved**: browser-like. h1 underlined+bold, h2 bold with more top margin, h3-h6 proportionally smaller margins; hr rendered via border-top-style: solid; blockquote gets left border + padding; em/strong/a already present. |
| Q-5 | Selection / clipboard support | **Resolved, extended by q-text-selection**: Ctrl-C copies focused link URL via OSC 52 to terminal clipboard. Full text range-selection added — see q-text-selection. |
| Q-6 | Mouse support level | **Resolved**: click (link/form) + scroll wheel (3 rows/tick). No drag-to-resize. |
| Q-7 | HTTPS via cpp-httplib + OpenSSL | **Resolved**: already implemented — CPPHTTPLIB_OPENSSL_SUPPORT enabled in cmake/HttpLib.cmake, OpenSSL linked; Url::parse accepts https:// and maps port to 443; httplib::Client auto-selects SSLClient. |
| Q-8 | Charset support beyond UTF-8 | **Resolved**: POSIX iconv from glibc/libc (no new dep); Content-Type header > meta prescan > assume UTF-8. See ADR 003. |
| Q-9 | Cookies / sessions | **Resolved, extended by q-persisted-cookies**: in-memory session CookieJar (RFC 6265 §5 simplified); no Secure/SameSite enforcement; per-SharedBrowsingState (shared across tabs). Disk persistence added — see q-persisted-cookies. |
| Q-10 | `:hover` semantics in terminal | **Resolved**: mouse-over hit-test (`BrowserView::update_hover`) rebuilds a single-node hovered set, re-resolves style, relayouts, repaints (`restyle_for_hover`). No ancestor chain (only the directly hovered node matches `:hover`, not ancestors) — covers the common `a:hover`/`.btn:hover` case. No keyboard equivalent. |
| Q-11 | Anchor navigation animation policy | **Resolved**: instant — `#fragment` jumps directly to the anchor row with no animation. |
| Q-12 | Debug overlay (Ctrl-D) detail | **Resolved**: box outlines in magenta (Ctrl-D toggle); focus-order labels and box-dim display deferred. |
| Q-13 | `<table>` support | **Resolved**: flex-reuse layout in UA stylesheet (ADR 002); no colspan/rowspan. |
| Q-14 | Error page styling | **Resolved**: UA-themed — body padding + p margin applied via inline style in error_page_html(). |
| Q-15 | CI host | **Resolved**: GitHub Actions — `.github/workflows/ci.yml`, Ubuntu latest, clang/Ninja, cmake+ctest. |
| Q-16 | License | **Resolved**: MIT — `LICENSE` file at repo root, copyright Tomas Macik 2026. |
| Q-17 | tvision pin | **Resolved**: pinned to commit `9a7a6439` in `cmake/Tvision.cmake` via `TVSHOW_TVISION_TAG`. |
| Q-18 | Image renderer plug interface | Resolved: `ImageRenderer::render(int cols, int rows, std::string_view alt, std::string_view src) → vector<string>`. v1 impl: `AltTextRenderer` (writes `[alt]` in row 0). |
| Q-19 | Config file location and format | **Resolved**: `~/.config/tvshow/config.toml` (TOML subset); CLI flags override. XDG_CONFIG_HOME respected. |
| Q-20 | Form `enctype: multipart/form-data` | Deferred (tied to file upload, needs file dialog). |
| Q-21 | Navigation history UX | **Resolved**: back/forward stack (`navigate_back`/`navigate_forward`), exposed via Alt-Left/Alt-Right (status line + menu). Window title shows `(pos/total)` history position, appended alongside scroll `[pct%]`. |
| Q-22 | Debug overlay enrichment | **Resolved**: Ctrl-D overlay draws box `WxH` dims top-right of each outlined box (`draw_box_outline`), and a focus-order index (`0,1,2,...`) at the origin of each link/form-control span, in `total_focusables()` order (`apply_focus_order_labels`). |
| Q-23 | `<img>` ASCII-art renderer | **Resolved, extended by q-ascii-art-renderer**: `stb_image.h` vendored (ADR-004) decodes fetched `<img src>` bytes into `Page::images` (`load_page(..., fetch_images=true)`); `BrailleRenderer` quantizes to 2x4-dot braille glyphs, falling back to `AltTextRenderer` on any cache miss/decode failure. Renderer choice: `image-renderer = "alt" | "braille"` in `config.toml`, or `--image-renderer=alt|braille` CLI flag (default `alt` -- image fetch/decode only runs when braille is selected, avoiding the cost otherwise). Third mode `ascii` added — see q-ascii-art-renderer. |
| Q-24 | Table column alignment | **Resolved**: `compute_table_col_widths()` measures max cell content-width per column index across all rows (handles `thead`/`tbody`/`tfoot` wrappers), stashed thread-locally (`g_table_col_widths`) and consulted by `layout_flex()` per `<tr>` in place of per-row flex-basis. |
| Q-25 | CSS `position: relative/absolute` | **Resolved**: `position` in §7.2 (`Position` enum, `top`/`left_offset`/`right_offset`/`bottom` on `ComputedStyle`). `relative` offsets from normal-flow position without affecting siblings. `absolute` is removed from flow (no space reserved) and offsets against the nearest ancestor with `position != static` (`apply_position_offsets` tracks a containing-block box separately from the immediate parent), falling back to the viewport when none exists. `fixed`/`sticky` are rendered as viewport-pinned overlays, not part of normal flow positioning — see Q-29. |
| Q-26 | Scroll position indicator | **Resolved**: window title shows `[pct%]` scroll position when content exceeds viewport height (`BrowserView::sync_vscroll()`). |
| Q-27 | Table `colspan`/`rowspan` | **Resolved**: `compute_table_grid()` (renamed from `compute_table_col_widths()`) runs the classic HTML column-assignment algorithm — cells are placed left-to-right, skipping grid columns still occupied by an earlier row's rowspan, tracked via a `rows_occupied_after` vector. Colspanning cells sum the widths of every column they cover (+ inter-column gaps); rowspanning cells get their box height extended post-layout (`apply_table_rowspans()`) once every row's actual height is known. Tables with any span disable `td`'s per-row `flex-grow` stretch (`TableGrid::has_span`) — otherwise rows with fewer items than real columns (because a span ate a slot) would grow disproportionately and drift out of alignment; unspanned tables keep the original stretch-to-fill behavior. `border-collapse` stays out of scope. |
| Q-28 | Form `enctype: multipart/form-data` | **Resolved**: `FormControlKind::File` (`<input type="file">`) opens tvision's `TFileDialog` on Enter/click (`BrowserView::show_file_picker`), storing the picked full path in `form_values_.text[node]` (reusing the generic text-value store) -- the field shows just the filename, `[Choose File]` when unset. `submit_form()` detects any file field via `layout::collect_form_files()`; if present (and method is POST -- a GET can't carry file content), it reads each picked file from disk, and `layout::encode_multipart()` builds the `multipart/form-data` body. `net::HttpClient::post()` gained a `content_type` parameter (previously hardcoded to urlencoded in `CppHttpClient`'s `httplib::Client::Post()` call) to carry the boundary through. A file that can't be read (missing/permission) is silently skipped, same degrade-gracefully pattern as the rest of the fetch/submit paths. |
| Q-29 | `position: fixed`/`sticky` layer model | **Resolved**: `layout::Box::overlays` (populated only on the root `Box`) holds one `OverlayBox` per fixed/sticky element -- laid out standalone at local origin (0,0) so `render::render(overlay.box)` yields just its own content, plus a `pinned_origin` resolved from `top`/`left`/`right`/`bottom` against the viewport. `Fixed` elements are fully out of flow (dropped from the main tree, matching the old drop behavior but now visible); `Sticky` elements stay in flow (their row is reserved normally) *and* get an overlay entry. `BrowserView::draw()` composites pinned overlays onto a fresh viewport-sized frame every call (`CharGrid::blit`), independent of `scroll_row_` for fixed; sticky is pinned once `scroll_row_` has scrolled its normal-flow row (`static_doc_row`, translated through `kept_rows_` into collapsed/visual row space) above `pinned_origin.row`, static before that. Scoped to **block-level children only** — a fixed/sticky element that's a direct child of a flex container is still dropped (unchanged from before), since flex item sizing doesn't have a natural place to carry overlay boxes and this is a rare pattern in practice (nav bars/headers are block-level). |
| Q-30 | Expand out-of-subset elements | **Resolved**: `iframe`/`video`/`audio` are synthesized into a token run (`emit_media_placeholder()` in `inline_text.cpp`) carrying `src` as the token's `href` -- `[Embedded: <src>]` for `iframe`, `[Media: <src>]` for `video`/`audio` (falling back to the first `<source src>` child if the element has no `src` of its own). This makes it a real, focusable, Enter-navigable link for free: `collect_links()` and the browser's Tab/Enter handling are already generic over any token with a non-empty `href`, no `<a>`-specific code touched. An element with no resolvable `src` at all contributes nothing (same as before). `canvas`/`svg` stay out of scope (no static src to point at). |
| q-text-selection | Text selection (mouse/keyboard range-select + copy) | **Resolved (decision)**: extends Q-5 — adopt full text range-selection alongside existing Ctrl-C link-URL copy. Implementation (selection model, mouse-drag/keyboard-extend UX, clipboard write path) deferred to ADR + code. |
| q-download-manager | Download manager (save fetched resource to disk, progress, list) | **Resolved (decision)**: "Save Link As" on focused link/media token via `TFileDialog`, pre-filled with last-used save dir (persisted to `config.toml`, `download-dir` key). Synchronous fetch, no progress bar, no history list — see `adr-download-manager`. |
| q-ascii-art-renderer | `<img>` ASCII-art renderer | **Resolved (decision)**: extends Q-23 — add `ascii` as a third `image-renderer` mode alongside `alt`/`braille`, same pluggable `ImageRenderer` interface (Q-18). Glyph/gradient choice deferred to ADR + code. |
| q-dev-tools | Dev tools (console/network/DOM inspector) | **Resolved (decision)**: extends Q-12/Q-22 — add a Network Log overlay (method/URL/status/bytes/ms, ring-buffer, newest-first) alongside the existing box-outline debug overlay. No JS console (no JS engine), no live DOM tree (box-outline overlay already exposes box dims) — see `adr-dev-tools`. |
| q-extensions-api | Extensions API | **Resolved (decision)**: no in-process plugin ABI (rejected — undercuts `adr-multiprocess-sandboxing`'s risk posture). Three process-boundary mechanisms instead: (1) config-driven content/ad blocklist, no execution — `adr-content-blocklist`; (2) fire-and-forget external handlers (video/media, mailcap-style spawn) — `adr-external-handlers`; (3) embedded 3rd-party windows (calculator/translation) via subprocess + line-based stdin/stdout, `TWindow`-owned chrome — `adr-external-window-provider`. |
| q-persisted-cookies | Persisted cookies (disk-backed, survive restart) | **Resolved (decision)**: extends Q-9 — add disk persistence to existing in-memory `CookieJar`. Storage format/location, Secure/SameSite enforcement still deferred to ADR + code. |
| q-websocket-support | WebSocket support | **Resolved (decision)**: won't implement now — conditioned on a scripting layer entering scope (nothing today can consume a pushed message; §1.2 non-goal: no JS). Revisit trigger = scripting layer exists — see `adr-websocket-support`. |
| q-multiprocess-sandboxing | Multi-process / sandboxing between tabs | **Resolved (decision): denied.** Disproportionate architecture change (IPC, process-per-tab) vs project mission (§1.1). Risk accepted — existing pure-pipeline/no-throw/degrade-gracefully design already narrows blast radius without process isolation — see `adr-multiprocess-sandboxing`. |

### 20.1 Milestone 1 (M1) Scope

All seven M1 items are resolved: ~~M1-nav-history~~, ~~M1-scroll-indicator~~, ~~M1-hover~~,
~~M1-debug-overlay~~, ~~M1-table-cols~~, ~~M1-css-position~~, ~~M1-img-renderer~~ — see Q-21,
Q-26, Q-10, Q-22, Q-24, Q-25, Q-23 above. **M1 complete.**

### 20.2 Milestone 2 (M2) Scope

All four M2 items are resolved: ~~M2-table-span~~, ~~M2-fixed-sticky~~, ~~M2-out-of-subset~~,
~~M2-file-upload~~ — see Q-27, Q-29, Q-30, Q-28 above. **M2 complete.**

---

## 21. Versioning of This Spec

This document is the contract for v1. Any change to a Locked Decision (§2) or to subsets (§6, §7) requires:
1. ADR in `docs/decisions/`.
2. SPEC update in the same commit.
3. Test updates reflecting the new behavior.
