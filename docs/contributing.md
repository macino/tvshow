# tvshow — Contributor Guide

---

## Architecture overview

tvshow's render path is a pure functional pipeline. Each stage takes a value and returns a new value with no side effects. This makes every stage independently unit-testable without a terminal.

```
bytes ──▶ DOM ──▶ CSSOM ──▶ Styled DOM ──▶ Box Tree ──▶ CharGrid
                                                              │
                                                    tvision TDrawBuffer
```

| Stage | Module | Pure? | Input → Output |
|-------|--------|-------|----------------|
| net | `net/` | No | URL → bytes + headers |
| dom | `dom/` | Yes | bytes → `dom::Document` |
| css | `css/` | Yes | stylesheet text → `css::Stylesheet` |
| style | `style/` | Yes | Document + Stylesheets → `style::StyledNode` tree |
| layout | `layout/` | Yes | StyledNode tree + Viewport → `layout::Box` tree |
| render | `render/` | Yes | Box tree → `render::CharGrid` |
| paint | `paint/` | No | CharGrid + `TDrawBuffer` → terminal output |

Pure stages have no I/O, no globals, no thread state. Anything impure lives in `net/`, `paint/`, `ui/`, `app/`, or `util/log`.

---

## Repository layout

```
tvshow/
├── client/
│   ├── include/tvshow/    # one public header per module
│   └── src/
│       ├── app/           # TApplication, MDI windows, menus
│       ├── net/           # HttpClient interface + cpp-httplib impl
│       ├── dom/           # Gumbo → our DOM types
│       ├── css/           # Katana → CSSOM, selector matching, cascade
│       ├── style/         # computed-style resolver, UA stylesheet
│       ├── layout/        # block/inline/flex → Box tree
│       ├── render/        # Box tree → CharGrid
│       ├── paint/         # CharGrid → TDrawBuffer
│       ├── images/        # ImageRenderer interface, AltTextRenderer
│       └── util/          # Url, percent-encode, Color, logging
├── server/
│   ├── src/main.cpp       # cpp-httplib router
│   └── pages/             # sample HTML/CSS pages
├── tests/
│   ├── unit/              # doctest cases per module
│   ├── golden/            # *.html → *.grid snapshots
│   └── integration/       # server + client end-to-end
├── docs/
│   └── decisions/         # Architecture Decision Records
├── cmake/                 # toolchain helpers, FetchContent wrappers
└── third_party/           # vendored header-only deps
```

---

## Build system

CMake ≥ 3.20 with three presets:

| Preset | Flags | Use |
|--------|-------|-----|
| `debug` | `-O0 -g` | Development, unit tests |
| `asan` | `-O1 -fsanitize=address,undefined` | Memory/UB checking |
| `release` | `-O3 -DNDEBUG` | Optimizer-level issue catching |

```sh
cmake --preset debug
cmake --build --preset debug

# Or build a specific target
cmake --build --preset debug --target tvshow-tests
```

Compiled dependencies (`tvision`, `gumbo-parser`, `katana-parser`) are pulled via `FetchContent` and pinned to specific commits in `cmake/`. Header-only dependencies (`cpp-httplib`, `doctest`) are vendored under `third_party/`.

---

## Test discipline

**Test-first is mandatory for all pure modules.** The workflow is:

1. Write a failing doctest case against the public header.
2. Confirm it fails for the right reason.
3. Implement the minimum to make it pass.
4. Refactor under green.

### Test layers

**Unit tests** (`tests/unit/<module>/`) — doctest cases. Run with:
```sh
ctest --preset debug --output-on-failure
```

**Golden tests** (`tests/golden/`) — HTML fixtures rendered to `CharGrid` snapshots. Each fixture is a pair `name.html` + `name.grid`. Regenerate snapshots after an intentional rendering change:
```sh
UPDATE_GOLDEN=1 ctest --preset debug -R "golden"
```
Always diff the regenerated `.grid` file before staging it.

**Integration tests** (`tests/integration/`) — spawn `tvshow-srv` on an ephemeral port, drive the client as a library (no terminal), assert on the resulting `CharGrid`.

### Running the full CI gate

```sh
./scripts/ci.sh
```

This runs, in order and fail-fast:
1. `cmake --preset debug` (configure)
2. `cmake --build --preset debug`
3. `clang-format --dry-run -Werror` on all `.cpp`/`.hpp`
4. `clang-tidy` with `WarningsAsErrors: '*'`
5. `ctest --preset debug`
6. `cmake --preset asan` + build + `ctest --preset asan`
7. `cmake --preset release` + build

All gates must be green before a change is considered done.

---

## Definition of done

A change is done when every one of these holds:

- New behavior has a deterministic automated test that fails without the change.
- All existing tests pass.
- `clang-format` reports no diffs.
- `clang-tidy` reports no new warnings on touched files.
- Debug and Release builds are clean (no new warnings).
- `ctest` is green, including sanitizer-instrumented runs.
- If a SPEC subset (§6/§7) was touched: SPEC updated in the same commit.
- If a structural decision was made: ADR added under `docs/decisions/`.

---

## Adding a feature

### 1. Check SPEC scope

Read the relevant SPEC sections. If the feature would broaden the HTML subset (§6), the CSS subset (§7), or change a Locked Decision (§2), file an Open Question in SPEC §20 first and get it resolved before writing code.

### 2. Write the test first

For a pure module change, add a doctest case to `tests/unit/<module>/`. For a visual change, add an HTML fixture to `tests/golden/` and generate a `.grid` snapshot with `UPDATE_GOLDEN=1`.

### 3. Implement

Implement the minimum to make the test pass. No speculative APIs or extra parameters "for later."

### 4. Run CI

```sh
./scripts/ci.sh
```

Fix every failure before continuing.

### 5. Write an ADR if needed

Structural decisions — new dependencies, pipeline boundary changes, locked-decision changes — need an ADR in `docs/decisions/`. Copy `docs/decisions/template.md`, start with status `proposed`, and flip to `accepted` in the same commit as the implementing code.

---

## Extending the HTML/CSS subset

The supported subsets are defined in SPEC §6 (HTML) and §7 (CSS). Do not broaden them on impulse:

- Found an unsupported tag in a fixture? Either it's in scope (write a test) or it isn't (verify it degrades gracefully — that's also tested).
- Need a new CSS property? Open a SPEC §20 Question first, then SPEC update + ADR, then code.

Unsupported tags are parsed and skipped; their children are rendered as if their parent were `div`. Unsupported CSS properties are parsed and silently ignored.

---

## Coding conventions

- **C++20.** Use `std::expected`-shaped result types for expected errors. No exceptions across module boundaries.
- **One public header per module** under `client/include/tvshow/<module>/`.
- **No globals** beyond what `tvision` requires.
- **No I/O in pure stages.** `dom`, `css`, `style`, `layout`, `render` must remain pure (no network, disk, stdout, time, randomness).
- **clang-format.** Config at repo root. Run before every commit.
- **clang-tidy.** Config at repo root. `WarningsAsErrors: '*'` — treat every new warning as a build failure.
- **Comments.** Only when the *why* is non-obvious: a hidden constraint, a subtle invariant, a workaround. Don't narrate what the code does.

---

## Adding a third-party dependency

Adding any new dependency requires an ADR under `docs/decisions/`. Do not pull in a new library because "it would be nice."

Header-only deps: vendor under `third_party/` and commit.
Compiled deps: add a `FetchContent` declaration in `cmake/`, pinned to a specific commit hash.

---

## Architecture Decision Records

ADRs live in `docs/decisions/NNN-short-title.md`. Copy `docs/decisions/template.md`. Statuses: `proposed` → `accepted` → `deprecated` / `superseded by NNN`. Commit ADRs alongside the implementing code. Never delete a reversed ADR; supersede it.

---

## Module boundaries cheat sheet

| What you're touching | Allowed dependencies |
|----------------------|----------------------|
| `dom`, `css`, `style`, `layout`, `render` | STL, each other (layered), nothing from `app/`, `paint/`, `net/` |
| `images/` | STL only (interface definition); impls may depend on `render/` |
| `net/` | STL, `cpp-httplib`, `util/url` |
| `paint/` | `render/`, tvision |
| `app/`, `ui/` | Everything |
| `util/` | STL only |
