# adr-websocket-support — deferred, conditioned on a scripting layer

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-websocket-support, §1.2 (Non-Goals — JavaScript execution)

## Context

q-websocket-support: `net/` is HTTP/1.1 request-response only (`cpp-httplib`). SPEC §1.2 lists
JavaScript execution as a v1 non-goal — and with no scripting layer, nothing in the client can
*consume* a WebSocket message once it arrives (no event loop hook, no DOM mutation API to react
to pushed data). Building the transport without a consumer is dead weight.

## Decision

Won't implement now. Condition, not permanent rejection: revisit if/when a scripting layer (or
equivalent server-push consumption mechanism) enters scope. Until then, a WebSocket client would
need a new dependency and a rearchitected event loop (current model is synchronous
request-response inside `TApplication`'s idle loop, §3.1) with no code path to use the result —
not worth building ahead of the need.

## Consequences

- No new dependency, no event-loop rearchitecture now.
- Revisit trigger is explicit and checkable: existence of a scripting/consumption layer, not a
  vague "later."
- `net/` stays HTTP/1.1-only per §2 Locked Decisions until this is revisited.

## Alternatives Considered

- **Build WebSocket transport now, wire consumption later**: rejected — no way to validate the
  API shape without a real consumer; likely gets redesigned anyway once scripting lands.
- **Server-Sent Events (SSE) as a lighter alternative**: same blocker (no consumer) — same
  deferral applies, not pursued separately.

## References

- SPEC §20 q-websocket-support, §1.2, §3.1
