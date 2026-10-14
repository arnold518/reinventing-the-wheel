# Single-cycle core stage-order layout

The complete `RV32ISingleCycleCore` now has an intentional floorplan:
Fetch, Decode, Execute, Memory, Writeback from left to right, with Next PC
and execution status below. Generic connectivity layout cannot infer that order
from a graph containing writeback, PC and transaction-control feedback loops.
No pins, wires, hierarchy, execution behavior or stage interiors were changed.

`server._single_cycle_core_layout` fits actual child aspect ratios and pin
footprints. It is shared by native Restore Children and automatic-refresh
classification. Unknown child sets fall back to generic graph layout. The core
default aspect is 0.52; its stage defaults and the entire pipeline are unchanged.

`tools/generate-single-cycle-core-layout.py` regenerates only core type/profile/
root entries, and recenters CORE in the 22 existing single-cycle program fixtures.
Memories, clocks, colors and unrelated layouts are preserved. Construction-only
topologies avoid allocating simulation histories. Repeating generation is
byte-identical.

Old shipped positions are retained in migration history. Browsers migrate
unchanged defaults to the new row order and proportions; manually moved/resized
children, custom proportions/colors and entries marked manual are retained.
The WASM Restore Children action uses the generated floorplan. A broad automatic
refresh recognizes the core as a custom layout and cannot reorder its stages.

## Validation

- Five focused CTests passed: single-cycle floorplan geometry, RV32I layout
  regressions, selective automatic refresh, and both layout-migration suites.
- Geometry checks cover normal and mixed child proportions, enlarged pin
  footprints, unknown topologies, row separation and deterministic regeneration.
- Browser checks passed for 18 focused scopes across the standalone core,
  structural program 3 and the unchanged pipeline. They start with the previous
  saved layout and verify stage order, proportions, bounds, non-overlap and the
  actual Restore Children action. Core screenshots were visually inspected.
- A before/after comparison confirmed that only the core type/profile, two
  standalone root entries and CORE placement in 22 program roots changed.
  Every stage interior and every pipeline layout remained identical.
- The strict documentation build passed. The local port 8893 serves the new
  layout/migration assets, and the production static bundle has the same assets.

This was layout-only validation; no full simulator rebuild or full test-suite
run was needed. The simulator binaries, topology and execution contracts did
not change.
