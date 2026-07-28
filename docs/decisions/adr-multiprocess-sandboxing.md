# adr-multiprocess-sandboxing — denied

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-multiprocess-sandboxing, §1.1 (Mission)

## Context

q-multiprocess-sandboxing: client is single-process; one tab's fault affects the whole app.
Real GUI browsers isolate tabs via process-per-tab + sandboxing for security and crash isolation.

## Decision

Denied. Process-per-tab (IPC, sandboxing, process lifecycle management) is a major architecture
change disproportionate to this project's mission — a hostable demo stack for a defined HTML/CSS
subset (§1.1), not a hardened multi-tenant browser. Risk accepted as a known limitation.

Mitigations already in place, no new work needed: pipeline stages are pure functions with no
shared mutable state (§3.1), module-boundary code doesn't throw exceptions (CLAUDE.md Style),
and fetch/parse/render paths degrade gracefully (skip/fallback) rather than crash on malformed
input — the existing architecture already narrows the blast radius of a single bad page without
process isolation.

## Consequences

- No IPC layer, no process-per-tab, no sandboxing — explicitly out of scope going forward.
- A crash in one tab's render/layout path can still affect the whole application; accepted risk.
- If this project's scope ever grows toward untrusted multi-tenant use, this decision would need
  revisiting from scratch (not an incremental extension of current architecture).

## Alternatives Considered

- **Thread-per-tab (lighter than process) isolation**: still doesn't provide crash isolation
  (shared address space) — rejected, doesn't solve the stated problem.
- **Partial sandboxing (e.g. seccomp on the fetch path only)**: rejected — adds real complexity
  for a partial guarantee; not proportionate given accepted-risk framing above.

## References

- SPEC §20 q-multiprocess-sandboxing, §1.1, §3.1
