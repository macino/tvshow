# tvshow — Agent Guide

Operational guidance for AI agents (Claude, Copilot, others) working in this repository.

## Project

Terminal "web browser" rendered with TurboVision. Server (HTTP/1.1) returns HTML+CSS; client parses, lays out into character cells, paints with `tvision`. See **`SPEC.md`** for the full contract.

> Read `SPEC.md` first. Treat it as binding. Any deviation requires an ADR under `docs/decisions/` plus a SPEC update in the same commit.

## ai-wkf

This repository follows the workflow rules at `/home/tomas/vcs/ai-wkf`. Apply them to every session:
- `ai-wkf/CLAUDE.md` — primary operational rules
- `ai-wkf/knowledge/ce-ee-communication-style.md` — communication style

Critical inherited rules:
- **Caveman mode (full)** — permanent default for chat output. Code, commit messages, and SPEC text remain normal.
- **CE/EE communication** — direct, no pre-announcements, flag blockers first, promise only when done.
- **No unsanctioned commits** — never `git commit` / push / delete without explicit user approval.
- **No AI attribution in commits** — no `Co-authored-by: Claude/Copilot/...`.
- **Approval gates** — pause before any destructive op.
- **Scope discipline** — explicit requirements only. Out-of-scope findings → document, don't execute.
- **Underspecified request** → ask clarifying questions before guessing.

## Build / Run / Test

```
# build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# run
./build/server/tvshow-srv --port 8080
./build/client/tvshow http://localhost:8080/

# test
ctest --test-dir build --output-on-failure
```

Targets: `tvshow` (client), `tvshow-srv` (demo server), `tvshow-tests` (unit + golden), `tvshow-itests` (integration).

## Test-First Discipline

The pipeline (`net → dom → css → style → layout → render`) is split into pure modules so behavior can be asserted without a terminal:

1. Write the doctest case **first**, against the public header of the target module.
2. Make it fail.
3. Implement the minimum to make it pass.
4. Refactor under green tests.

For pixel/grid behavior, prefer **golden tests** (`tests/golden/`): an HTML+CSS fixture with a stored `CharGrid` snapshot. Diff failures show as a side-by-side grid in the test output.

Do not reach for a terminal to verify rendering. The render layer is a pure function: `(BoxTree, Viewport) → CharGrid`. If you cannot test a behavior without a terminal, the seam is wrong — fix the seam.

## Module Boundaries

Stages marked **pure** in SPEC §3.1 must remain free of:
- I/O (network, disk, stdout, stderr).
- Globals or `TApplication` references.
- Time, randomness, threads.

Anything impure lives in `net/`, `paint/`, `ui/`, `app/`, or `util/log`. Adding I/O to a pure module is a SPEC violation.

## Dependency Rules

- Header-only deps (`cpp-httplib`, `doctest`) are vendored under `third_party/`.
- Compiled deps (`tvision`, `gumbo-parser`, `katana-parser`) are pulled via CMake `FetchContent`, pinned to a specific commit.
- Adding a new third-party dep requires an ADR. Don't pull anything new because "it would be nice."

## Subset Boundary

The HTML and CSS subsets are defined in SPEC §6 and §7. Don't broaden them on impulse:
- Encountering an unsupported tag or property in a fixture? Either it's in scope (write a test), or it isn't (ignore it gracefully — that's also tested).
- Need a new property? Open Question entry first, then SPEC update + ADR, then code.

## Style

- C++20. `clang-format` and `clang-tidy` configs live at the repo root; run them before requesting review.
- One public header per module under `client/include/tvshow/<module>/`.
- No exceptions across module boundaries. Use `std::expected`-shaped result types for expected errors.
- No globals beyond what `tvision` requires.

## Communication

- Caveman + CE/EE for chat.
- Commit messages: normal English, conventional-commit-shaped subject (`feat:`, `fix:`, `refactor:`, `docs:`, `test:`, `build:`, `chore:`), body explains *why* when not obvious. No AI attribution.
- This is a one-person project. No code review step exists; the sieve is automated tests (see `EXECUTION.md`).

## Architecture Decisions

ID scheme (per ai-wkf slug-ID rule, `ai-wkf/CLAUDE.md` §Workflow Rules — Generated IDs): new ADRs use `docs/decisions/adr-<slug>.md` (slug = first significant words of title, kebab-case), no number. ADRs 001–004 predate this rule and stay numbered — frozen, never renumbered (code/tests cite them by number). Statuses: `proposed` → `accepted` → `deprecated` / `superseded by <id>`. Commit alongside implementing code. Do not delete reversed ADRs — supersede them.

Same split applies to SPEC §20 Open Questions: `Q-1`..`Q-30` are frozen numeric IDs (cited in code/tests, never renumbered); `Q-31`+ use slug IDs (`q-<slug>`). See SPEC §20 header note.

## Where Things Live

| You want to... | Look here |
|----------------|-----------|
| Understand the contract | `SPEC.md` |
| Add a feature | corresponding `client/src/<module>/`, with test next to it |
| Add an HTML/CSS feature | SPEC §6/§7 first → ADR → code |
| Add a sample page | `server/pages/` and reference it from `tvshow-srv` route table |
| Record a design decision | `docs/decisions/` |
| Find the next thing to carve | SPEC §20 (Open Questions) |
