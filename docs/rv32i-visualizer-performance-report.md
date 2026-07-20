# RV32I Visualizer Performance Report

## Scope

This optimization pass keeps the structural simulation and its visible signal contract exact. It does not replace gate-level components with behavioral components, omit pin or wire state, or introduce a coarse zoom mode that hides otherwise resolvable detail.

The Program 9 structural scenario used for measurement contains:

- 27,265 components;
- 96,827 pins;
- 53,761 wires;
- 141 recorded timestamps.

## Implemented Changes

### Lossless indexed state transport

The old state response used full hierarchical pin and wire IDs as JSON object keys on every timestamp. Those keys alone occupied 11,764,212 characters.

`VisualSignalSnapshot` now holds the already-discovered pin and wire handles in stable order and collects every value in one C++ call. The server returns index-aligned arrays. Topology entries expose the matching `stateIndex`, and the client retains compatibility with the old dictionary representation.

All four logic states remain exact: `0`, `1`, `X`, and `Z`. Multi-bit values retain their original most-significant-bit-first strings.

### Event-driven canvas rendering

The canvas no longer redraws continuously while idle. A frame is requested after camera, hover, selection, layout, state, memory-overlay, or resize changes. Continuous animation remains enabled during playback.

### Conservative sub-pixel hierarchy cutoff

The renderer stops descending only when a component is narrower than one physical display pixel. Its parent shell remains visible, and the complete hierarchy reappears automatically on zoom. The selected component, pin, or wire ownership path remains expanded even below this threshold.

This cutoff does not change simulation state and does not hide detail that the display can physically resolve.

### Transport compression and session bounds

Large JSON responses use gzip level 1 when the client advertises support. The scenario cache is an LRU with a default maximum of two full sessions, configurable through `VISUALIZER_V2_MAX_SESSIONS`.

Component overlay pin lookup is now indexed by `(component ID, pin name)` instead of scanning the complete pin table.

### Complete Python topology exposure

The structural system used `ConstantValue<2, 2>` for `CONST_WORD_SIZE`, `ConstantValue<4, 4>` for the eight trap-cause values, and `RV32IBitPatternMatcher` for 40 instruction decoders. These concrete C++ types were not registered with pybind11. The simulator constructed and connected them correctly, but Python saw only their base `Component` interface, so the visualizer omitted their pins.

The bindings now expose both constant specializations and the pattern matcher. The server also validates that every wire source and sink ID resolves to an exposed pin, turning future missing bindings into a clear scenario-load error instead of a silently incomplete drawing.

The live Program 9 topology now contains the width-2 `CONST_WORD_SIZE.OUT`, eight width-4 trap-cause outputs, and all 40 `INPUT[32]`/`MATCH[1]` matcher boundaries. All wire endpoints resolve.

### Collision-free dense decode layouts

`RV32IBitPatternMatcher` instances do not all have the same internal topology: their mask and value determine which inverter gates exist and how many terms enter the AND chain. Reusing one type-level child layout mixed placements from different instruction patterns. Matcher layout keys now include the 32-bit mask and value, while shared base styling still comes from `RV32IBitPatternMatcher`.

The layered layout calculation also no longer forces a child width larger than the vertical or horizontal fit it already calculated. This matters for the eight tall 32-bit input splitters inside `IMMEDIATE_MUX`; their corrected default relative width is approximately `0.0292` rather than the overlapping `0.045`.

Defaults were regenerated for all 40 matcher configurations and `Mux8to1_32bit`. A recursive live audit of the 2,514 components under `CORE.DECODE_CONTROL` reports zero component-body overlaps, zero pin/stub overlaps, zero missing placements, and zero parent-boundary violations.

## Program 9 Measurements

Measurements were taken through the systemd visualizer on port 8765.

| Operation | Before | After |
|---|---:|---:|
| Timeline state generation and response | 11.4-13.3 s | 0.37-0.51 s |
| Timeline state, uncompressed | 12.85 MB | 632 KB |
| Timeline state over gzip | not enabled | 22-27 KB |
| Warm topology response | dominated by old full-state generation | 1.92 s |
| Topology over gzip | 66.7 MB uncompressed | 2.99 MB transferred |

Cold construction of the full structural Program 9 session still takes about 62.5 seconds. That cost is dominated by constructing and simulating the complete gate-level hierarchy, not by the now-optimized state transport. Future cold-start work should cache or share immutable structural topology metadata without replacing the structural simulation.

## Exactness Verification

The new indexed response was compared against pre-optimization dictionary responses at:

- timestamp index 0;
- index 114, after the ADDI checkpoint;
- index 140, after the illegal-instruction trap.

All 96,738 pins exposed at the time and all 53,761 wires matched at all three points: zero mismatches. The later binding-completeness repair exposed 89 previously omitted boundary pins without changing the C++ circuit or its simulation; the current pin count is 96,827.

Additional verification:

- indexed snapshot equivalence on a small circuit at three timestamps;
- gzip response headers and browser decompression;
- headless Chrome load and render of structural Program 9;
- JavaScript and Python syntax checks;
- topology integrity audit of 103 distinct canonical circuit families, with zero missing endpoints, duplicate IDs, unregistered connected types, or pin/wire width mismatches;
- a dedicated `VisualizerModuleBindingsTest` regression for 2-bit constants, 4-bit constants, and the RV32I pattern matcher;
- default-layout collision regression for a full-width ECALL matcher and `Mux8to1_32bit`, including pin/stub clearance;
- 143/143 CTest targets passing.

## Remaining Conservative Opportunities

- Cache immutable encoded topology per structural type.
- Share topology metadata among structural Programs 1-16 while keeping their simulator histories independent.
- Recompute geometry and wire routes only for a moved subtree during layout editing.
- Add performance counters for frame time, visible objects, payload size, and state latency.

Aggressive hierarchy hiding, behavioral substitution, and mandatory WebGL rendering are deliberately outside this pass.
