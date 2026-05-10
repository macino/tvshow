# tvshow — Execution Plan

How work gets done on this project. Pairs with `SPEC.md` (the contract) and `CLAUDE.md` (the agent guide).

## 0. Operating Model

- **One developer (the user).** No team, no PR review, no design review board.
- **AI agents do the implementation.** The user directs (chooses milestones, answers Open Questions, approves commits) and occasionally spot-checks the running app at their convenience.
- **The sieve is automated.** Because no human reviews the diff line-by-line, every change must pass a comprehensive automated check before it can be considered done. If the tests don't catch it, it isn't caught.

This document defines exactly what "passes" means and how iterations are structured around that bar.

---

## 1. Cadence

User check-ins are infrequent and asynchronous. Between check-ins the agent operates in this loop:

```
┌─▶ pick next unblocked task ──▶ confirm SPEC scope ──▶ open Q? ─yes─▶ pause, ask user
│                                                         │
│                                                         no
│                                                         ▼
│                                       write failing tests (red)
│                                                         │
│                                                         ▼
│                                       implement minimum (green)
│                                                         │
│                                                         ▼
│                                       refactor under green
│                                                         │
│                                                         ▼
│                                       run local CI (§5) — all gates green?
│                                                         │
│                                          ┌── no ────────┘
│                                          │              │
│                                          ▼              ▼ yes
│                                     diagnose,      stage work, ask user for commit approval
│                                     fix, retry          │
│                                          │              ▼
└──────────────────────────────────────────┘    on approval: commit, push if asked, mark task complete
```

Idle = no committable work in progress. The agent does not invent scope to fill time; it reports status (§7) and stops.

---

## 2. Per-Iteration Loop (Detailed)

For each task the agent picks up:

1. **Re-read SPEC scope.** Identify which sections (§) the task touches. If it would broaden a subset (§6/§7) or change a Locked Decision (§2), stop and open an Open Question or ADR before coding.
2. **Check Open Questions.** If the task is gated on any Q-* in SPEC §20, ask the user; do not guess.
3. **Write failing tests first.** New module → unit tests in the module's `tests/` dir. Behavior crossing modules → golden test under `tests/golden/`. End-to-end change → integration test under `tests/integration/`.
4. **Make tests fail meaningfully.** Confirm the failure message points at the right thing. Bad red = bad green.
5. **Implement the minimum.** Just enough to flip every new test green. No speculative interfaces, no extra params "for later."
6. **Refactor under green.** Only if it improves clarity *and* every test still passes.
7. **Run the local CI script (§5).** All gates must be green.
8. **Update task tracking + memory** (§11, §12).
9. **Pause for commit approval** (§8). Do not commit unprompted.

---

## 3. Definition of Done (Universal)

A change is **done** when *every one* of these holds:

- New behavior has a deterministic automated test that fails without the change and passes with it.
- All existing tests still pass.
- `clang-format` reports no diffs.
- `clang-tidy` (strict config) reports no new warnings on touched files.
- `cmake --build` is clean (no new warnings) on Debug and Release.
- `ctest` is green, including sanitizer-instrumented runs.
- If a SPEC subset (§6/§7) was touched: SPEC updated in the same change.
- If a structural decision was made: ADR added under `docs/decisions/`.
- Task entry updated, memory updated where applicable.

Anything short of all the above = the task stays in progress.

---

## 4. Automated Test Layers

The agent leans on these layers — they replace human review.

### 4.1 Static Layer (cheap, runs first)
- `clang-format --dry-run -Werror` on touched files.
- `clang-tidy` with `.clang-tidy` config; treats new warnings as errors on touched translation units.
- CMake configure + Debug build: zero warnings (`-Wall -Wextra -Wpedantic -Werror`).

### 4.2 Unit Layer (per pure module)
- doctest cases co-located with each module.
- Coverage gate: any new public function in a pure module (SPEC §3.1: `dom`, `css`, `style`, `layout`, `render`, plus pure helpers) needs a direct unit test.
- Sanitizer build: separate Debug build with `-fsanitize=address,undefined` runs the unit suite.

### 4.3 Property / Table Layer
- Color conversion (truecolor → 256 → 16) — table-driven tests covering palette boundaries, achromatic colors, alpha discard.
- CSS unit math (px/em/ch/% → cells) — table-driven, including rounding edge cases.
- Selector specificity — table-driven against the cascade spec.
- HTTP status routing (200/3xx hops/4xx/5xx) — table-driven against fake HttpClient.

### 4.4 Golden Grid Layer (the core "is the render right" replacement for human eyes)
- Each fixture: `tests/golden/<name>.html` (+ optional `<name>.css`, `<name>.viewport`).
- Pipeline runs end-to-end through `render` → produces a `CharGrid`.
- `CharGrid` serialized in a stable text format:
  - Top half: chars in a `cols × rows` grid.
  - Bottom half: per-cell attribute codes (fg, bg, style flags) in the same grid layout.
- Snapshot stored at `tests/golden/<name>.grid`.
- On diff, the test prints both grids side-by-side with a per-cell diff mask.
- Update flow: `UPDATE_GOLDEN=1 ctest` regenerates snapshots; agent must inspect the diff before staging.

### 4.5 Integration Layer
- Spawns `tvshow-srv` on an ephemeral port via a test fixture.
- Drives the client *as a library* (no terminal): builds an in-memory `TBrowserWindow` with a stubbed display, asserts on the resulting `CharGrid` and on input-event responses.
- Covers: navigation push/pop, form submit (GET + POST), redirect chains, error pages, multi-tab state isolation.

### 4.6 Determinism / Stability
- Time, randomness, network — all behind interfaces with deterministic test impls.
- Color autodetection stubbed to a fixed mode in tests.
- `tvision` headless display driver used in tests (no real terminal).

### 4.7 Optional / Future Layers (not gates yet)
- Fuzz harness for HTML and CSS parsers (libFuzzer entrypoints under `tests/fuzz/`). Run on demand, not in CI initially.
- Coverage report (`gcov`/`llvm-cov`) — generated locally, not gated. Useful to spot dead seams.
- Mutation testing — deferred. Mentioned for completeness; not pursued unless tests start passing for the wrong reasons.

---

## 5. Local CI Script

A single command must run every gate. The agent uses this before claiming done:

```
./scripts/ci.sh
```

`scripts/ci.sh` runs, in order, fail-fast:

1. `cmake --preset debug` (configure)
2. `cmake --build --preset debug --warnings-as-errors`
3. `clang-format --dry-run -Werror $(git ls-files '*.cpp' '*.hpp')`
4. `run-clang-tidy -p build/debug -warnings-as-errors='*' <touched files>`
5. `ctest --preset debug --output-on-failure`
6. `cmake --preset asan` + build + `ctest --preset asan`
7. `cmake --preset release` + build (catch optimizer-level issues)

Every gate green = task may proceed to commit approval. Anything red = the agent diagnoses, fixes, re-runs from step 1. The agent does not selectively rerun; the cheap-first ordering already minimizes wasted time.

CMake presets and the script are themselves built in **Milestone 0** before any feature work.

---

## 6. Reporting to User

Reports follow CE/EE rules: terse, fact-cited, no pre-announcements.

### 6.1 After a finished task (pre-commit, awaiting approval)
```
Task #N done.
- Tests added: tests/unit/css/specificity_test.cpp (+47 lines, +12 cases)
- Files touched: client/src/css/cascade.cpp (+82/-3), client/include/tvshow/css/cascade.hpp (+9/-0)
- ci.sh: green (debug, asan, release)
- New ADR: none
- SPEC change: none
- Open Q raised: none
Ready to commit. Approve?
```

### 6.2 Status digest (idle / between tasks)
```
Done since last check-in: M2 net layer (commits abc1234..def5678), M3 DOM adapter (ghi9012).
In progress: M4 CSS adapter — selector matcher (red, 3/12 cases passing).
Blocked: none.
Next without input: M4 continue.
Need input: Q-1 (address bar UX) before M13 starts.
```

### 6.3 Blocker
Reported immediately, not after attempting a workaround:
```
Blocked: tvision FetchContent build fails on missing `ncursesw` headers.
Cannot proceed with M0. Need: install libncursesw5-dev OR switch to vendored tvision build.
```

### 6.4 Caveman intensity in reports
Caveman/CE-EE rules apply. Numbers, hashes, file paths win over prose. No "Let me…", no "I'll…".

---

## 7. Approval Gates

Hard rules, no exceptions:

| Action | Gate |
|--------|------|
| `git commit` | Explicit user approval per commit. |
| `git push` | Explicit user approval per push. |
| `git reset --hard`, `git rebase`, `git branch -D` | Explicit per-action approval. |
| Adding a third-party dep | ADR + user approval. |
| Broadening HTML/CSS subset | SPEC update + user approval. |
| Touching `~/` outside the repo (config, cache schema) | User approval, even for read. |
| Networking outside `localhost` | Forbidden. |

Implicit / no approval needed:
- Edits to tracked files, including new files inside the repo.
- Running `ci.sh`, `ctest`, `cmake`.
- Running `tvshow-srv` and `tvshow` against `localhost`.
- Reading anything in the repo.

---

## 8. Open-Question Protocol

Open Questions live in **SPEC §20**. Lifecycle:

1. **Discover** — agent realizes a task can't proceed without a decision. Adds a Q-N entry to SPEC §20 if not already there. *Does not invent an answer.*
2. **Surface** — at the next user check-in (or immediately if blocking current iteration), the agent reports the question with: context, the options, the agent's recommendation, the trade-off.
3. **Decide** — user picks. Agent records the decision in the appropriate SPEC section and either removes the Q-N entry or marks it `resolved: <answer>`.
4. **ADR (if structural)** — see §9.

Order of preference when stuck:
- Question already answered in SPEC → follow it.
- Question implicitly answered by ai-wkf rules → follow them.
- Otherwise → stop, ask. Never paper over with a temporary guess.

---

## 9. ADR Protocol

ADRs live in `docs/decisions/`. Triggers:

- Adding/removing a third-party dep.
- Changing a Locked Decision (SPEC §2).
- Changing pipeline stage boundaries (SPEC §3.1) or making a "pure" stage impure.
- Picking among the Open Questions when the answer has structural impact.
- Reversing a previous ADR (new ADR with `supersedes: NNN`).

Process:
1. Copy `decisions/template.md` (created in M0) to `decisions/NNN-short-title.md`.
2. Status starts `proposed`.
3. Commit alongside the implementing change → status flips to `accepted`.
4. Reversal: new ADR, both updated; never delete the old one.

---

## 10. Task Tracking

- The Claude Code task tool (`TaskCreate`/`TaskUpdate`) is used for the **current iteration's** todos. Don't use it as long-term backlog.
- Long-term backlog = milestone roadmap (§13) plus SPEC §20 Open Questions.
- Per-task scratch artifacts (notes, `wip.patch`, partial logs) live under `tasks/{task_id}/` and are gitignored. This mirrors the ai-wkf convention.
- Cross-cutting learnings (e.g. "tvision quirk on resize") go to `docs/knowledge/*.md`, not into a task folder.

---

## 11. Knowledge Persistence

- `SPEC.md` — contract.
- `CLAUDE.md` — agent guide.
- `EXECUTION.md` — this file.
- `docs/decisions/` — ADRs.
- `docs/knowledge/` — durable learnings (terminal capability quirks, library gotchas).
- Claude memory (`/home/tomas/.claude/projects/-home-tomas-vcs-tvshow/memory/`) — session-spanning facts about user preferences, project priorities. *Not* a place for code knowledge.
- `tasks/{task_id}/` — ephemeral per-task notes.

If a finding repeats across tasks, it belongs in `docs/knowledge/`. If it changes how all future agents should behave, it belongs in `CLAUDE.md`.

---

## 12. Milestone Roadmap

Each milestone has a Definition of Done (DoD) that is *fully verifiable by the automated layers in §4*. No milestone closes on visual inspection.

### M0 — Scaffolding
**DoD:** Repo builds an empty TApplication that opens, shows menu/status line, and exits cleanly. `ci.sh` exists and passes (no tests yet — the gate is "no warnings, exits 0"). CMake presets `debug`, `asan`, `release` defined. `clang-format` and `clang-tidy` configs landed. `decisions/template.md` landed.

### M1 — Foundation Types
**DoD:** `CharGrid` (cell = char + `TColorAttr`), `Viewport`, `CellRect`, `Url` types implemented with full unit coverage. `CharGrid` serialization (the golden snapshot format) implemented and tested.

### M2 — Net Layer (mockable)
**DoD:** `HttpClient` interface + `cpp-httplib` impl + `FakeHttpClient`. Redirect chain handling, error mapping, charset detection. Unit + table tests; no real network in tests.

### M3 — DOM (Gumbo Adapter)
**DoD:** `dom::parse(bytes) → Document` returns our DOM types from Gumbo output. Skipped tags handled per SPEC §6.6. Unit tests on the documented subset, including malformed input.

### M4 — CSS (Katana Adapter + Cascade)
**DoD:** `css::parse_stylesheet` produces a `Stylesheet`; selector matcher supports the SPEC §7 selector list; cascade + specificity + `!important` produce a deterministic computed-style map. Inline styles, `<style>`, `<link>` external sheets all merge correctly. Table tests on specificity and the !important rule.

### M5 — Computed Style Resolver
**DoD:** Given DOM + Stylesheet list → a styled-DOM where every node has its computed style. UA defaults applied first. Unit tests for inheritance (inherited vs reset properties), defaults, unit conversion (px/em/ch/%).

### M6 — Layout (Block Flow)
**DoD:** Block-level boxes positioned correctly: stacking, margin collapse (vertical adjacent siblings only), width resolution, padding, border reservation. Unit + golden tests for representative documents.

### M7 — Render (chars + colors)
**DoD:** Box tree → `CharGrid` honoring color, bg, bold/italic/underline. Truecolor / 256 / 16 fallbacks tested via injected color depth. First end-to-end golden test (HTML+CSS → grid) passes.

### M8 — Paint Adapter (tvision side)
**DoD:** `paint::draw(CharGrid, TDrawBuffer*)` that the headless test display driver consumes. First running binary that loads a static file and renders it (no HTTP yet). Integration test asserts the grid; user can `cmake --build && ./tvshow file://...` to spot-check.

### M9 — Borders & Box-Drawing
**DoD:** SPEC §9 mapping implemented; border-color honored; collapse to `none` when width=0. Golden tests for each border style.

### M10 — Inline Flow & Wrapping
**DoD:** Line boxes; word wrap; `white-space: pre|nowrap|normal`; `text-align`. Inline elements (`<span>`, `<a>`, `<b>`, `<i>`, etc.). Golden tests including pathological wrap cases.

### M11 — Links, Focus, History (single tab)
**DoD:** Tab/Shift-Tab traversal of focusables; visual focus; Enter on link triggers navigation event; history stack push/pop with Back/Forward. Integration tests drive key events through the headless display.

### M12 — HTTP Fetch + Demo Server
**DoD:** `tvshow-srv` serves the SPEC §16 sample pages. Client navigates against the running server in integration tests (ephemeral port). Error pages rendered on 4xx/5xx.

### M13 — Address Bar
**DoD:** Ctrl-L modal dialog accepts URLs and triggers navigation. Validates against `Url` type. Integration test for typed URL → fetch → render. (Open Q-1 must be resolved or v1 commits to modal.)

### M14 — Form Controls
**DoD:** SPEC §13.2 controls render and accept input. Tab traversal includes them. Unit + golden tests per control. No submit yet.

### M15 — Form Submission
**DoD:** GET appends query string; POST sends `application/x-www-form-urlencoded`. Submit replaces current document, history pushed. Demo server `/echo` round-trip integration test.

### M16 — MDI Tabs
**DoD:** Multiple `TBrowserWindow` instances coexist as MDI children. Window menu lists tabs. Per-tab history isolation. Cascade/Tile commands. Integration test asserts state isolation across two tabs.

### M17 — Flex Layout
**DoD:** SPEC §10.3 flex subset. Golden tests for each `justify-content` and `align-items` combination, with `gap`, `flex-grow`, `flex-shrink`, `flex-basis`. (Open Q-3 must be resolved.)

### M18 — Image Renderer Interface (alt-text impl)
**DoD:** `images::ImageRenderer` interface defined; `AltTextRenderer` is the v1 impl. Width/height attrs reserve cells correctly per SPEC §10. Future ASCII-art renderer can be plugged without touching layout. (Open Q-18 must be resolved.)

### M19 — Resize & Scrolling Polish
**DoD:** Terminal resize re-layouts from cached styled DOM. Native `TScrollBar` reflects content vs viewport. PageUp/Down, Home/End, anchor navigation (`#fragment`). Integration tests for resize and anchor scroll.

### M20 — Corpus Hardening
**DoD:** Golden test corpus expanded to cover every SPEC §6/§7 feature explicitly. Sanitizer suite green on the full corpus. Release build size and startup time recorded as a baseline metric. Open Questions resolved or explicitly punted to v1.1 with ADRs.

**Out of v1:** tables (Q-13), HTTPS (Q-7), cookies/sessions (Q-9), bookmarks (Q-2), real images, JS.

---

## 13. Recovery & Blockers

- **Test fails for unclear reason** → don't disable, don't `--no-verify` past it, don't tweak the test until it passes. Diagnose. If genuinely stuck, write a minimal repro and report as a blocker.
- **Dependency build breaks** → report; never silently swap to a different lib (would violate Locked Decisions).
- **Found a real bug outside current task scope** → record in `tasks/{task_id}/out-of-scope.md`, surface in next status report. Do not fix in the current change.
- **Accidental destructive op (e.g. `rm` ran on wrong path)** → stop, report, do not attempt recovery without user direction.
- **CI script too slow to be practical** → flag it as a meta-task; don't bypass it.

---

## 14. Manual Spot-Check Protocol (User-Driven)

Spot-checks are at the user's convenience, not scheduled. To make them frictionless:

- `tvshow` launches against an arg URL and exits cleanly on Alt-X.
- `tvshow-srv` runs from `./build/server/tvshow-srv --port 8080` with no other setup.
- A `make demo` (or `scripts/demo.sh`) target builds, starts the server in the background, and launches the client at `http://localhost:8080/`.
- `scripts/demo.sh stop` shuts down the demo server cleanly.
- Logs go to `~/.cache/tvshow/log` so spot-checks don't pollute the terminal.

If a spot-check surfaces a defect the automated tests didn't catch, the agent's first job on the next task is to **add the missing test layer** before fixing the defect itself. The test gap is the more important bug.

---

## 15. Document Versioning

This file evolves alongside SPEC. Material changes to the methodology (a new test layer, a new approval gate, a milestone re-ordering) require:
- Edit here.
- ADR if the change is structural.
- A note in the status report at the next user check-in.
