# Two-panel memory view and floorplan for every shared arrangement

Started: 2026-10-21. Status: closed (2026-10-21). Code state: branch
`realistic-timing` at `c6382c0`. Follows the
[content-area record](2026-10-21-layout-content-area.md).

## Summary

The owner found the D-0 and D-ID scenarios oddly laid out and asked why
their memory had no two-panel view. Both are fixed.
- The inspector now tells instruction and data accesses apart behind a
  shared cache too, by the side whose request the arbiter has in flight.
- One floorplan now serves all four shared arrangements. The core and the
  memory sit in the same place in each, the middle parts are spread evenly,
  and each cache's constant sits by its cache. The memory is wide enough for
  its two panels at the root's zoom.

## 2026-10-21 — Changes and checks

- **Why one panel.** On 2026-10-20 the two-panel view was turned off behind
  a shared cache. The inspector classified a memory request by the arbiter's
  grant at the edge the memory took it, which no longer lines up once the L2
  sits in between. But the L2 serves one request at a time, and its fills
  and write-throughs to memory happen while the arbiter holds that request,
  whose side the arbiter's `STATE.OWNER_D` bit names (set when the arbiter
  accepts). `MemoryInspector` now uses `OWNER_D` when a `SHARED_CACHE`
  sibling exists, and the grant otherwise. Behind a compact arbiter (no
  `STATE` part) the memory stays one panel. The public header is unchanged:
  in this case the inspector keeps the arbiter's `STATE` part in place of
  the arbiter itself.
- **Floorplan.** The C-ID special case is removed. One branch places the
  core (0.28 of the content width), then whichever of the L1 pair, the
  arbiter, the L2 and the memory (0.34) are present, with equal gaps. Cache
  constants go just above their cache (the data L1's just below). The
  earlier memory width (about 205 px at root zoom) was below the two-panel
  drawing's minimum (`drawUnifiedMemoryContents` needs 200 px and 90 px per
  panel), so even B showed an empty box. All four roots are recomputed.
- **Docs.** §4.7 explains how D-0 and D-ID tell the sides apart, and that a
  line the L2 fetched for one side can later serve the other.
- **Checks (pass):**
  - Native: `MemoryInspectorIntegrationTest` (now two panels in B, D-0 and
    D-ID, with code below 0x100 on the instruction side and data from 0x100
    on the data side), `VisualizerStaticLayoutTest` (with the content-area
    guard), `RV32IMemoryLayoutTest`, `CacheLayoutTest`,
    `VisualizerAutomaticLayoutTest`, `VisualizerScenarioCatalogTest`,
    `MemoryInspectorTest`.
  - Pages build.
  - Browser: the four shared scenarios load with role `unified`, and
    screenshots of D-0 and D-ID show both panels; `MemoryInspectorBrowserTest`,
    `MemoryInspectorWasmParityTest`, `VisualizerScenarioWasmParityTest`,
    `VisualizerScenarioBrowserTest`.
