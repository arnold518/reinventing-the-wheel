# Child placements inside their parent's content area

Started: 2026-10-21. Status: closed (2026-10-21). Code state: branch
`realistic-timing` at `17a2640`. Follows the
[memory systems record](2026-10-20-memory-systems-section.md).

## Summary

The owner saw parts placed over title bars in the new memory-arrangement
scenarios. A check of every saved placement against the parent's content
area found 11 placements, in five layouts, outside it:
- the four shared-memory program roots (the core, clock and memory in the
  side margins; in D-ID also a constant over the title bar);
- `mux2to1-32bit` (one inverter overlapping the title bar).

All are fixed. `tools/check-static-layouts.py` (`VisualizerStaticLayoutTest`)
now fails on any placement outside a content area, in every scenario.

## 2026-10-21 — Cause, fix and checks

- **The content area**, as `contentRect` in `app.js` draws it: below the
  title bar (`title_bar_ratio` × the parent's width, so `title_bar_ratio /
  aspect` of its height) and between side margins of `boundary_area_ratio`,
  where pins are drawn.
- **Causes.**
  - The unified program-root floorplan (C-ID, written 2026-10-19, and its
    generalization of 2026-10-20) placed children on the whole width,
    ignoring the margins.
  - The script that saved the three new roots passed a fixed title fraction
    (0.02) and margin (0.035) instead of each root's ratios. The D-ID
    constant landed on the title bar.
  - `mux2to1-32bit` had an older type-level placement of `NOT_SEL` at
    y 0.163 against a title fraction of 0.197.
- **Fix.**
  - New `_content_placer` in `visualizer-v2/server.py` places on a 0..1
    scale across the content width, keeps each child's aspect, and clamps it
    below the title bar and above the bottom. Both unified floorplan branches
    use it; the columns are slightly tighter, so D-ID's memory keeps a usable
    width.
  - The four roots (C-ID included) are recomputed with their real title,
    margin and pin ratios and saved.
  - `NOT_SEL` gets a scenario-root override at y 0.200, which clears the
    next row at 0.25.
- **Guard.** `check-static-layouts.py` checks each placement against its
  parent's content area, using the layout manager's aspect and ratios
  (cached per layout key). Run on the committed `layout.json`, it reports the
  10 placements in the four scenarios it was given; on the fixed file, 0.
- **Checks (pass):** `VisualizerStaticLayoutTest` with the new guard (every
  scenario, 0 problems), `RV32IMemoryLayoutTest`, `RV32ISingleCycleLayoutTest`,
  `CacheLayoutTest`, `VisualizerAutomaticLayoutTest`,
  `VisualizerScenarioCatalogTest`. Pages build. In the browser, the four
  shared-memory scenarios load with their parts inside the root's content
  area, as screenshots show.
