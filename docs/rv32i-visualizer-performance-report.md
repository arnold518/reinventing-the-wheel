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

### Progressive exact topology loading

The full structural RV32I topology grew to 54,049 components, 180,155 pins,
and 103,361 wires after the unified component migration. Sending every pin and
wire descriptor before drawing the root view produced a 128.8 MB JSON response
and made the browser construct geometry and spatial-index entries for details
far below one physical pixel.

The initial response now contains:

- the complete, compact component tree required by the explorer;
- the complete indexed simulation state for the initial timestamp, while the
  server session retains the complete time-travel history;
- root and immediate-child pins;
- root-owned wires; and
- the full component/pin/wire counts.

The browser requests additional exact signal scopes from `POST /api/topology`
in batches when a component becomes resolvable on screen or lies on the path
to a selected component. A scope contains the owner's wires and every owner or
immediate-child pin needed by those wires. The server validates component IDs,
deduplicates requests, and rejects batches larger than 512 owners.

No simulated component, signal value, or time-travel history is removed.
Unloaded data is drawing metadata only. Zooming or focusing a deep component
fetches its real pins and wires before showing the detail.

Spatial indexes are likewise built only for the currently resolvable
hierarchy. Previously loaded scopes remain available, so returning to a
component does not discard work.

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

The original decoder used 40 independent full-instruction matcher
configurations and contained 2,514 components. Its factored replacement uses
22 shared opcode/function/exact predicates and contains 1,964 components.
Defaults were regenerated for the new predicates, recognition terms, and
`Mux8to1_32bit`. The deterministic layout overlap audit passes for the updated
decoder hierarchy.

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

## Progressive Loading Measurements

Measurements on the current 54,049-component Program 1 topology:

| Measurement | Eager topology | Progressive topology |
|---|---:|---:|
| Initial uncompressed JSON | 128,808,325 bytes | 17,853,122 bytes |
| Initial browser gzip transfer | several MB | 765,414 bytes |
| Pins in initial drawing scope | 180,155 | 53 |
| Wires in initial drawing scope | 103,361 | 29 |
| Python JSON parse | 4.28 s / 444 MB peak | 0.35 s / 79 MB peak |
| Clean local headless-browser overview | 14.7 s | 2.8 s |

The progressive payload is 7.2 times smaller before compression and roughly
168 times smaller over gzip than the former uncompressed response. A deep
depth-8 NAND gate focus loaded its missing ancestor/detail scopes, showed all
two inputs and one output, and completed without a browser error. A temporary
profile-store browser test also completed:

```text
MemoryBit structural: 28 components, 89 pins, 55 wires
MemoryBit behavioral: 1 component, 5 pins, 5 wires
MemoryBit restored:   28 components, 89 pins, 55 wires
```

Cold CPU construction and simulation remain intentionally exact and took
97.7 seconds in the isolated measurement process. The loading overlay now
distinguishes that phase from circuit-index decoding and overview preparation,
and it animates while the request is pending.

## Exactness Verification

The new indexed response was compared against pre-optimization dictionary responses at:

- timestamp index 0;
- index 114, after the ADDI checkpoint;
- index 140, after the illegal-instruction trap.

All 96,738 pins exposed at the time and all 53,761 wires matched at all three
points: zero mismatches. The later binding-completeness repair exposed 89
previously omitted boundary pins without changing the C++ circuit or its
simulation, bringing that topology to 96,827 pins. After the unified profile
migration expanded the structural register hierarchy, the live Program 9
topology contains 54,049 components, 180,155 pins, and 103,361 wires; it uses
the same lossless indexed snapshot path.

Additional verification:

- indexed snapshot equivalence on a small circuit at three timestamps;
- gzip response headers and browser decompression;
- headless Chrome load and render of structural Program 9;
- JavaScript and Python syntax checks;
- topology integrity audit of 103 distinct canonical circuit families, with zero missing endpoints, duplicate IDs, unregistered connected types, or pin/wire width mismatches;
- a dedicated `VisualizerModuleBindingsTest` regression for 2-bit constants, 4-bit constants, and the RV32I pattern matcher;
- default-layout collision regression for a full-width ECALL matcher and `Mux8to1_32bit`, including pin/stub clearance;
- 143/143 CTest targets passing.

After the unified component, test, profile-explorer, and progressive-topology
migrations were combined, the final repository regression passed 127/127
logical tests in 535.15 seconds. The smaller logical-test count reflects
consolidated fidelity-independent scenarios rather than reduced contract
coverage.

## Remaining Conservative Opportunities

- Cache immutable encoded topology per structural type.
- Share topology metadata among structural Programs 1-16 while keeping their simulator histories independent.
- Recompute geometry and wire routes only for a moved subtree during layout editing.
- Add performance counters for frame time, visible objects, payload size, and state latency.

Aggressive hierarchy hiding, behavioral substitution, and mandatory WebGL rendering are deliberately outside this pass.
