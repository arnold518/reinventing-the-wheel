# Visualizer layouts shared by parts of different structure

Started: 2026-10-19. Status: open (browser checks pending). Code state: branch `realistic-timing` at
`06f44ca`.

## Summary

The owner reported that `TAG_STORE_1` of the scalable two-way cache had a
clearly broken layout and asked to find and fix similar issues. All four SRAM
stores of that scenario were drawn squeezed into a strip about 2 % of the
device width. Two defects combined: SRAM floorplans were computed before
their children's proportions were known, and one saved floorplan was shared by
parts of different internal structure (an 8-row and a 16-row SRAM). The scan
found six more shared keys. Fixed with structure keys, a seeding fix, a
formatter fix, regenerated placements and a regression guard.

## 2026-10-19 — Investigation

- Layout lookup for a non-root part uses its layout key (type, or a special
  key) plus the build-profile fingerprint; the static (WebAssembly) site
  draws only placements saved in `visualizer-v2/layout.json`.
- `profile_layouts.0445f98e83c802f0.SRAM<32>`: 73 children within
  x 0.489–0.511 (storage bank width 0.015, drivers 0.0003);
  `SRAMStorageBank<32,8,2>` in the same profile within x 0.4965–0.5035. The
  16-row bank in the same profile was correct (0.07–0.93).
- **Cause 1 (seeding).** `_default_child_layouts` seeded children's type
  proportions before computing a floorplan only for memory-device, cache and
  RV32I floorplans, not for SRAM ones. The first `SRAM<32>` and 8-row bank met
  were placed with placeholder child proportions; the floorplan's fit-inside
  rule then scaled everything down to a strip. The 16-row bank, met later,
  was correct.
- **Cause 2 (shared key).** Every 32-bit SRAM with that profile shared one
  saved floorplan whatever its rows. A scan of all 339 scenarios (topologies
  only) for keys used by parts with different children found:
  `SRAM<32>` (8- and 16-row banks), `FFArray8` and `FFArray32` (1-, 2- and
  4-word stores in the two-level cache), `RV32IBranchPredictor` (three
  policies), `RV32IPredictionTable<16>` (one-bit and two-bit),
  `RV32IPredictionEntry` (the one-bit entry drawn with the two-bit layout:
  its two children crammed into 0.31 × 0.21 of the box), and
  `MemoryTimer<33,3>` (two counter structures).
- **Cause 3 (formatter).** `LayoutManager.save` compacted every `[...]` in the
  JSON text, including inside strings, so a key such as
  `SRAM<32>[SRAMStorageBank<32,8,2>]` was saved as
  `...<32, 8, 2>]`. The existing cache keys survived only because they hold
  no commas.

## 2026-10-19 — Fix and verification

- **Structure keys** (`_component_layout_type` in `visualizer-v2/server.py`,
  `componentLayoutType` in `src/visualizer/VisualizerSession.cpp`): SRAM and
  FF memory devices append their storage bank's layout type; the cache rule
  (hash of child names) now also covers prediction tables and entries, the
  branch predictor and memory timers. Placements under the bare type are not
  merged into a structure key (server, `app.js`, static check). Defaults of a
  structure key come from its bare type.
- **Seeding:** SRAM floorplans now seed their children's proportions first.
- **Formatter:** strings are left unchanged; the shipped file round-trips
  byte-identically.
- **Placements:** a one-off migration (topologies only) created the 19 new
  keys from the replaced keys' settings and computed their placements with
  seeded proportions (also a generic type-level copy for other profiles),
  recomputed the squeezed 8-row bank, and removed 10 profile entries no part
  reads any more. The 32-bit SRAMs now span x 0.08–0.93 (8 rows) and
  0.18–0.83 (16 rows, scaled to its taller bank); the bank 0.07–0.93.
- **Guard:** `tools/check-static-layouts.py` also fails on a key shared by
  parts with different children (compared by child names and plain types)
  and on an SRAM floorplan narrower than 0.35 of its parent. Run against the
  previous `layout.json` with the previous key rule, it reports both defects
  for the scalable cache scenario.
- **Tests updated:** `FFMemoryBindingsTest`, `FFMemoryBrowserTest` (new FF
  key), `VisualizerScenarioCatalogTest` (C++ and Python keys agree for the
  scalable cache, the FF memory and the branch predictor).
- **Checks run** (native `VisualizerSessionProbe` and `circuit_backend`
  rebuilt in a 950 MiB scope): `VisualizerScenarioCatalogTest`,
  `RegisterLayoutGenerationTest`, `RV32IMemoryLayoutTest`,
  `RV32ISingleCycleLayoutTest`, `SRAMLayoutTest`, `MemoryDeviceLayoutTest`,
  `CacheLayoutTest`, `VisualizerModuleBindingsTest`, `FFMemoryBindingsTest`,
  `VisualizerSessionContractTest`, `VisualizerAutomaticLayoutTest`,
  `VisualizerAutomaticLayoutMigrationTest`, `VisualizerStaticLayoutTest`
  (0 problems in 339 scenarios): 13/13 pass.
- **Not yet run:** the WebAssembly build with the C++ change and the browser
  checks (`FFMemoryBrowserTest`, `SRAMLayoutBrowserTest`, the cache
  examples); they join the memory-gated run. The live `visualizer-v2`
  backend module was not rebuilt (isolated build only); the native server's
  keys come from Python and do not need it.
