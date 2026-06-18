# tvshow — Embedding Guide

tvshow's render pipeline is a set of pure C++20 modules. You can use them independently in your own project — for example to render HTML+CSS to a character grid in a custom TUI, a server-side terminal preview, or a test harness.

---

## Which modules are embeddable

| Module | Header | Pure? | What it does |
|--------|--------|-------|--------------|
| `dom` | `tvshow/dom/parser.hpp`, `node.hpp` | Yes | Parses HTML bytes into a DOM tree |
| `css` | `tvshow/css/parser.hpp`, `types.hpp` | Yes | Parses CSS text into a stylesheet |
| `style` | `tvshow/style/resolver.hpp`, `tree.hpp` | Yes | Resolves computed styles (cascade + inheritance) |
| `layout` | `tvshow/layout/engine.hpp`, `box.hpp`, `types.hpp` | Yes | Lays out styled nodes into a box tree |
| `render` | `tvshow/render/render.hpp`, `chargrid.hpp` | Yes | Renders a box tree to a `CharGrid` |
| `images` | `tvshow/images/renderer.hpp` | Yes | `ImageRenderer` interface — plug your own impl |

The full pipeline is:

```
dom::parse() → style::resolve() → layout::layout() → render::render()
```

CSS sheets are parsed separately with `css::parse()` and passed to `style::resolve()`.

---

## Adding tvshow to your CMake project

```cmake
include(FetchContent)
FetchContent_Declare(
  tvshow
  GIT_REPOSITORY https://gitlab.com/tomas.macik/tvshow.git
  GIT_TAG        <commit-hash>
)
FetchContent_MakeAvailable(tvshow)

target_link_libraries(your_target PRIVATE tvshow_core)
```

`tvshow_core` is the static library that contains all pure pipeline modules. It does not link tvision; you get the pipeline without the TUI dependency.

---

## Pipeline walkthrough

### 1. Parse HTML

```cpp
#include "tvshow/dom/parser.hpp"

std::string html = R"(
  <!doctype html><html><body>
    <style>h1 { color: #c00; }</style>
    <h1>Hello</h1>
  </body></html>
)";

auto doc = tvshow::dom::parse(html);
if (!doc) {
    // parse failed — doc is std::nullopt
}
```

`dom::parse` returns `std::optional<dom::Document>`. A `Document` holds a node tree plus a list of inline stylesheet texts found in `<style>` tags.

### 2. Parse stylesheets

```cpp
#include "tvshow/css/parser.hpp"

std::vector<tvshow::css::Stylesheet> sheets;

// Inline <style> blocks collected by the DOM parser
for (const auto& text : doc->inline_styles) {
    if (auto sheet = tvshow::css::parse(text)) {
        sheets.push_back(std::move(*sheet));
    }
}

// External sheets you fetched yourself
if (auto sheet = tvshow::css::parse(external_css_text)) {
    sheets.push_back(std::move(*sheet));
}
```

### 3. Resolve styles

```cpp
#include "tvshow/style/resolver.hpp"

auto tree = tvshow::style::resolve(*doc, sheets);
if (!tree) {
    // resolution failed
}
```

`style::resolve` returns `std::optional<style::StyledNode>` — a tree where every node carries its computed style after cascade, inheritance, and UA defaults.

### 4. Layout

```cpp
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"

tvshow::layout::Viewport vp{80, 24};  // cols × rows
tvshow::layout::Box root = tvshow::layout::layout(*tree, vp);
```

`layout::layout` returns a `Box` tree. Each `Box` has a `border_box` with `{origin, size}` in cell coordinates. The tree mirrors the DOM structure for block/flex containers and is flat for inline leaf nodes.

### 5. Render to CharGrid

```cpp
#include "tvshow/render/render.hpp"
#include "tvshow/render/chargrid.hpp"

tvshow::render::CharGrid grid = tvshow::render::render(root);
// grid.cols() == 80, grid.rows() == 24
```

`render::render` returns a `CharGrid`. Each cell holds a `char` and a `TColorAttr` (foreground, background, style flags).

### 6. Read the grid

```cpp
for (int row = 0; row < grid.rows(); ++row) {
    for (int col = 0; col < grid.cols(); ++col) {
        char ch        = grid.char_at(col, row);
        TColorAttr attr = grid.attr_at(col, row);
        // do something with ch and attr
    }
}
```

### 7. Serialize / compare (golden testing)

```cpp
std::string snapshot = grid.to_string();
// Write to file, compare in tests, diff on mismatch

auto restored = tvshow::render::CharGrid::from_string(snapshot);
```

The serialized format is a stable text format:

```
COLS=80 ROWS=24
<character rows>
===
<attribute rows (fg:bg:flags per cell, space-separated)>
```

---

## Custom image renderer

`render::render` accepts an optional `images::ImageRenderer` reference. The default is `images::AltTextRenderer` (renders `[alt text]`). Provide your own to draw ASCII art or any other representation:

```cpp
#include "tvshow/images/renderer.hpp"

class MyRenderer : public tvshow::images::ImageRenderer {
public:
    std::vector<std::string> render(
        int cols, int rows,
        std::string_view alt,
        std::string_view src) const override
    {
        std::vector<std::string> result(rows, std::string(cols, ' '));
        // fill result[0..rows-1] with exactly cols chars each
        // e.g. draw ASCII art from src, or fetch and dither
        return result;
    }
};

MyRenderer my_renderer;
tvshow::render::CharGrid grid = tvshow::render::render(root, my_renderer);
```

`render()` calls your renderer for each `<img>` box it encounters, passing the cell dimensions reserved by the layout stage and the element's `alt` and `src` attribute values.

---

## Constraints and guarantees

**Pure modules have no side effects.** `dom`, `css`, `style`, `layout`, `render` never touch the filesystem, network, stdout, or time. You can call them from any thread concurrently on independent inputs.

**No globals.** The modules are entirely self-contained. No static state, no singletons.

**Error handling.** Parse and resolve functions return `std::optional<T>` (or an `std::expected`-shaped type in future versions). Never throw across module boundaries.

**Determinism.** Given the same HTML, CSS, viewport, and color depth, the pipeline always produces the same `CharGrid`. Color depth is passed as a parameter to the resolver, defaulting to truecolor.

---

## Minimal self-contained example

```cpp
#include "tvshow/dom/parser.hpp"
#include "tvshow/css/parser.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/render/chargrid.hpp"

#include <iostream>
#include <string>

int main() {
    const std::string html = R"(
        <!doctype html><html><body>
          <style>p { color: #0af; }</style>
          <p>Hello, tvshow!</p>
        </body></html>
    )";

    auto doc = tvshow::dom::parse(html);
    if (!doc) return 1;

    std::vector<tvshow::css::Stylesheet> sheets;
    for (const auto& s : doc->inline_styles)
        if (auto sheet = tvshow::css::parse(s))
            sheets.push_back(std::move(*sheet));

    auto tree = tvshow::style::resolve(*doc, sheets);
    if (!tree) return 1;

    auto box = tvshow::layout::layout(*tree, {40, 5});
    auto grid = tvshow::render::render(box);

    std::cout << grid.to_string();
}
```

Output is the `CharGrid` serialized format — 5 rows of 40 chars followed by the attribute map.
