# Selective automatic-layout refresh

The broad refresh updates child positions and widths, not circuit wiring or
simulation behavior. It preserves existing colors, proportions, pin styles,
root settings and unrelated records.

## Selection and preservation

The layout engine and refresh tool share `_custom_child_layout`. A recognized
memory matrix, register-bank arrangement, CPU floorplan or other custom provider
is excluded from automatic reflow. Its descendants are still visited: preserving
a pipeline's stage arrangement does not freeze automatic circuitry inside a stage.

An entry marked `layout_mode: manual` is also excluded. Existing records whose
child names do not match an available topology are retained and reported as
unmatched, rather than overwritten using a different fidelity's structure.
Records not reached by the selected scenarios are listed as `unvisited` and
preserved as well; this includes retired component names retained in old stores.

Type, profile and scenario-root entries are evaluated independently. Profile
geometry resolves child sizes and pin styles through that profile's overrides.
Custom floorplan entries remain unchanged, including their migration metadata.

## Safe execution

Preview the inventory and validate geometry without saving:

```sh
/home/arnold/venv/bin/python visualizer-v2/recalculate_layouts.py \
  --automatic-only --dry-run --report /tmp/layout-refresh-preview.json
```

Remove `--dry-run` to save. `--scenarios rv32i-memory-lanes` restricts the refresh
for diagnosis. The old unrestricted reset mode is separate and should not be
used to preserve existing layout settings.

`create_layout_topology` constructs the selected component scenario directly
through its catalog/profile, without executing its stimuli. RV32I program
scenarios use construction-only program roots; one representative covers each
program family. Legacy fixture setup is accepted only if it processes no
simulator events. Only one topology is retained at a time, with no visualizer
session, waveform export, or history playback.

Every refreshed arrangement is checked for child bounds, parent-input/output
stub clearance and non-overlapping child pin footprints. Vertical clearance is
converted from child width units into parent height units; the parent's compact
pin style must not substitute for a larger child's pins. Custom floorplans are
preserved and checked separately by their existing layout tests.

## Browser migration

Automatic geometry has a separate `automatic_layout_revision`, including a
deterministic geometry hash, and an `automatic_layout_history` of prior child
placements. Existing structural-layout revision metadata remains intact.

On load, the browser upgrades only child positions and widths that still match
an earlier shipped default. Hand-moved or resized children and custom colors or
aspect ratios remain unchanged. A whole entry explicitly marked manual is never
migrated. Retained snapshots support browsers that skip intermediate releases.
An identical second refresh does not alter the saved file.

## Validation of the broad refresh

The initial selective refresh visited 180 representative scenarios (219,315
component instances, released between scenarios). It refreshed 228 automatic
entries; 204 had changed geometry. All 267 encountered custom entries remained
identical, as did the existing styles in all 571 type/root records.

Four root records containing both current and historical child names were left
untouched: `ff-memory-container`, `sram-container`, `rom-container`, and
`rv32i-register-file` with their saved profile suffixes. Three unvisited type
records (`MemoryController<container,32>`, `MemoryTransactionPort<32>`, and
`MemoryPayloadRegister<2>`) were also retained. Construction of 16/32-bit
controllers separately confirmed that these three use custom floorplans.

The write pass took 29.91 seconds with 162.1 MiB peak RSS and no process swaps.
A full second pass produced a byte-identical `layout.json`.

The ten focused CTest layout, migration and wire/Rewire geometry tests passed.
The real WASM browser migration test upgraded 206 saved type/root entries while
preserving a manually moved ALU child, a moved SRAM cell and a custom color.
All eight memory-lane input branches route forward at overview and focused zoom,
including after saving and reloading. The screenshots were checked as well.

The SRAM browser regression also passed: rows/columns, bitline alignment,
legacy migration, colors and manually moved cells remain intact. Extended
Rewire checks passed for unpackers, all 32 memory-lane checkpoints, all 116
instruction-field checkpoints and the pipeline control packer. The first
extended run received SIGTERM after the lane checks; the remaining field and
control-packer checks passed in a fresh browser process. No cause for that
external termination was established.
