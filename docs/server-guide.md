# tvshow — Server Author Guide

This guide is for developers building HTTP servers or web applications whose output is consumed by tvshow. It covers what HTML and CSS tvshow understands, how it maps them to character cells, and how to get the best rendering results.

---

## How tvshow consumes a page

tvshow fetches a URL with HTTP GET. It expects `Content-Type: text/html` (with optional `charset=`). It parses the document with Gumbo (an HTML5 parser), resolves CSS from `<link>` and `<style>` tags, lays out the result into a character-cell grid, and renders it using box-drawing glyphs and truecolor terminal attributes.

Each character cell is one column wide and one row tall. The browser window is typically 80–132 columns × 24–50 rows, depending on the terminal size and the number of open tabs.

### Pixel-to-cell mapping

tvshow maps CSS pixel values to cell coordinates using:

| Axis | Ratio |
|------|-------|
| Horizontal | 8 px = 1 column |
| Vertical | 16 px = 1 row |

Fractional values round half-up. This ratio affects `width`, `height`, `margin`, `padding`, `border-width`, `gap`, `flex-basis`, and image `width`/`height` attributes.

---

## Supported HTML (v1)

### Document structure

```html
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Page title</title>
    <link rel="stylesheet" href="/style.css">
    <style> /* inline CSS */ </style>
  </head>
  <body>
    <!-- content -->
  </body>
</html>
```

### Block-level elements

`div`, `p`, `h1`–`h6`, `hr`, `pre`, `blockquote`, `section`, `article`, `header`, `footer`, `nav`, `main`, `ul`, `ol`, `li`.

### Inline elements

`span`, `a`, `b`, `strong`, `i`, `em`, `u`, `code`, `br`, `small`.

### Forms

| Element | Notes |
|---------|-------|
| `<form action method>` | `method="get"` or `method="post"` |
| `<input type="text">` | Single-line text |
| `<input type="password">` | Characters hidden |
| `<input type="checkbox">` | Toggled with Space |
| `<input type="radio">` | Grouped by `name` |
| `<input type="submit">` | Submits the form |
| `<input type="hidden">` | Not rendered, value submitted |
| `<textarea>` | Multi-line, scrolls on overflow |
| `<select>` + `<option>` | Opens as a dropdown dialog |
| `<button>` | Focusable, submits on Enter |
| `<label>` | Associates with control via `for` |

Form encoding: `application/x-www-form-urlencoded` (the default). `enctype="multipart/form-data"` is not supported in v1.

### Images

```html
<img src="/image.png" alt="A diagram" width="320" height="160">
```

tvshow renders `[alt text]` in the space reserved by `width` × `height` (converted to cells via the px→cell ratio). The `src` is not fetched in v1.

If `width`/`height` are absent, the reserved space defaults to `alt.length + 2` columns × 1 row.

### Out of scope — graceful fallback

These tags are parsed but their rendering is degraded:

| Tag | Fallback |
|-----|---------|
| `<table>`, `<tr>`, `<td>`, `<th>` | Children rendered as block `div` (no column alignment) |
| `<script>` | Entirely ignored |
| `<iframe>`, `<video>`, `<audio>`, `<canvas>`, `<svg>` | Entirely ignored |

---

## Supported CSS (v1)

### Sources and cascade

tvshow applies CSS from these sources in cascade order (lowest to highest precedence):

1. User-agent stylesheet (built-in)
2. `<link rel="stylesheet" href="...">` (HTTP-fetched)
3. `<style>` blocks
4. Inline `style="..."`

Specificity and `!important` follow the CSS spec.

### Selectors

| Selector type | Example |
|--------------|---------|
| Tag | `p`, `div`, `h1` |
| Class | `.card`, `.btn-primary` |
| ID | `#header`, `#nav` |
| Descendant | `nav a`, `div p` |
| Child | `ul > li` |
| Attribute existence | `input[required]` |
| Pseudo-class | `:focus` (applied to focused links and controls) |

### Honored properties

**Color**

| Property | Values |
|----------|--------|
| `color` | Any CSS color (named, `#rgb`, `#rrggbb`, `rgb()`, `rgba()`) |
| `background-color` | Same |
| `background` | Color shorthand only; images ignored |

Alpha is discarded in v1. Colors are mapped to truecolor, 256-color, or 16-color depending on terminal capability — detected automatically at startup.

**Text**

| Property | Values |
|----------|--------|
| `font-weight` | `normal`, `bold` |
| `font-style` | `normal`, `italic` |
| `text-decoration` | `none`, `underline`, `line-through` |
| `text-align` | `left`, `right`, `center` |
| `white-space` | `normal`, `pre`, `nowrap` |

**Box model**

| Property | Values |
|----------|--------|
| `width`, `height` | `px`, `%`, `ch` |
| `min-width`, `max-width` | `px`, `%`, `ch` |
| `margin` | shorthand and individual sides, `px`, `%`, `auto` |
| `padding` | shorthand and individual sides, `px`, `%` |
| `border` | shorthand |
| `border-style` | `none`, `solid`, `double`, `dashed`, `dotted`* |
| `border-color` | Any CSS color |
| `border-width` | `px` (any value > 0 becomes 1 cell; 0 collapses to `none`) |

*`dotted`, `ridge`, `groove`, `inset`, `outset` fall back to `solid`.

**Display and layout**

| Property | Values |
|----------|--------|
| `display` | `block`, `inline`, `inline-block`, `flex`, `none` |
| `visibility` | `visible`, `hidden` |
| `overflow` | `visible`, `hidden`, `scroll`, `auto` |
| `flex-direction` | `row`, `column` |
| `justify-content` | `flex-start`, `flex-end`, `center`, `space-between`, `space-around` |
| `align-items` | `flex-start`, `flex-end`, `center`, `stretch` |
| `gap` | single value, `px` |
| `flex-grow`, `flex-shrink` | number |
| `flex-basis` | `px`, `%`, `auto` |

Flex wrapping (`flex-wrap`) is not supported; flex containers always behave as `nowrap`.

### CSS units

| Unit | Meaning |
|------|---------|
| `px` | Pixels, converted via 8 px = 1 col / 16 px = 1 row |
| `ch` | 1 column |
| `em` | 1 row (font size has no effect in a monospace terminal) |
| `rem` | 1 row |
| `%` | Percentage of containing block's content area |

### Ignored properties

Any property not listed above is parsed (so `style="..."` attributes don't error) but not applied. This includes: `font-size`, `font-family`, `line-height`, `letter-spacing`, `opacity`, `border-radius`, `box-shadow`, `background-image`, `transform`, `transition`, `animation`, and everything in CSS Grid.

---

## Borders and box-drawing glyphs

tvshow draws CSS borders using Unicode box-drawing characters:

| `border-style` | Characters used |
|----------------|-----------------|
| `solid` | `┌ ┐ └ ┘ ─ │` |
| `double` | `╔ ╗ ╚ ╝ ═ ║` |
| `dashed` | `┌ ┐ └ ┘ ╌ ╎` |
| `dotted` | falls back to `solid` |
| `none` | no characters drawn, no cell reserved |

Border width is always 1 cell regardless of the `px` value. Setting `border-width: 0` collapses to `none`.

---

## Tips for targeting tvshow

**Use explicit widths in cells, not pixels.** `width: 40ch` is exactly 40 columns. `width: 320px` is 40 columns (320 ÷ 8). Both are fine; `ch` is clearer.

**Prefer `color` and `background-color` over gradients or images.** Gradients and background images are silently ignored.

**Use `border-style: solid` or `double` for structure.** These look intentional in a terminal. Rounded corners don't exist; `border-radius` is ignored.

**Keep line lengths predictable.** The viewport is typically 80–132 columns. Content wider than the viewport clips; there is no horizontal scrollbar in v1.

**Test with `white-space: pre` for code blocks.** `<pre>` elements get `white-space: pre` by default, preserving indentation and newlines.

**Anchor fragments work.** `<a href="#section">` navigates to the element with `id="section"`. Give important sections an `id` attribute.

**Forms follow standard semantics.** Name your inputs with `name` attributes. The `action` URL receives a standard `application/x-www-form-urlencoded` body on POST.

---

## HTTP response requirements

| Header | Required value |
|--------|---------------|
| `Content-Type` | `text/html` (with optional `; charset=utf-8`) |

tvshow follows HTTP redirects (`301`, `302`, `303`, `307`, `308`) up to 5 hops. Error responses (`4xx`, `5xx`) are displayed as built-in error pages.

Character encoding: UTF-8 is assumed if not specified in `Content-Type` or a `<meta charset>` tag.
