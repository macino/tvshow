# 002 — Table layout via flex reuse (no colspan/rowspan)

- **Status:** accepted
- **Date:** 2026-06-23
- **Touches SPEC:** §6.6 → §6.6 / §6.7, Q-13

## Context

Tables are the most common layout structure on informational sites (Wikipedia, documentation pages).
The current UA stylesheet maps `td` to `display: inline`, which causes all cells to run together in
a single line with no column alignment. Open Q-13 deferred tables to v1.1.

## Decision

Map table structure to existing display types in the UA stylesheet — no new layout code:

| Element | UA display |
|---------|-----------|
| `table` | `block` + `border-style: solid` |
| `thead`, `tbody`, `tfoot` | `block` |
| `tr` | `flex; flex-direction: row` |
| `td`, `th` | `block; flex-grow: 1; padding: 8px` (1 ch each side) |

Each `<tr>` becomes a flex container; its `<td>/<th>` children each get `flex-grow: 1`, so the row
width is divided equally among the columns. Column widths are consistent across rows only when all
rows have the same number of cells — sufficient for the target subset (no `colspan`/`rowspan`).

The outer `border-style: solid` on `<table>` renders a box-drawing-char frame around the table.
No inner cell borders are added; cells are visually separated by 1ch padding on each side.

Subset boundary:
- Supported: `table`, `tr`, `td`, `th`, `thead`, `tbody`, `tfoot`, `caption`
- Not supported: `colspan`, `rowspan`, `colgroup`, `col`, CSS `border-collapse`

## Consequences

- **Positive:** zero new layout code; tables render immediately on all sites that use them without colspan.
- **Positive:** border-collapse is not needed — outer frame only.
- **Negative:** unequal column counts across rows produce misaligned columns.
- **Negative:** no per-column width control; author `width` on `td` is respected but overridden by flex distribution.
- **Neutral:** `caption` renders as a bold block above the table (UA font-weight:bold).

## Alternatives Considered

- **Dedicated table layout algorithm**: correct column-count synchronization and colspan, but weeks of work; deferred to v2.
- **Keep display:inline on td**: current behavior; unreadable on any real table.

## References

- SPEC §6.7, §9 (border binary rule)
- ADR 001 (vendored deps)
