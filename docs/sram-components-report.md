# SRAM implementation and validation

Date: 2026-09-17. Public reference (renumbered): [section 3.1.2](../site-docs/memory/3.1.2.md).

## Scope

The new `memory.sram.*` families model a single-port asynchronous SRAM using
explicit digital bitlines. Existing FF components, `SRAMContainer`, CPU memory,
and the engine's single-source wire model are preserved.

The public building blocks are `SRAMCell`, `SRAMRow`, `SRAMBitlinePair`,
`SRAMStorageBank<W>`, `SRAMRowDecoder`, `SRAMPrecharge`, `SRAMWriteDriver`,
`SRAMSenseAmplifier`, `SRAMColumnSelector<W>`, `SRAMAccessControl<W>`,
`SRAMOutputBuffer<W>`, and `SRAM<W>`.

- Width W is a template parameter: 8, 16, or 32 bits.
- `SRAMDeviceConfig` supplies word count N, words per row K, and phase timing.
  Rows = N/K; bit columns = W*K; capacity = N*W/8 bytes.
- Cells, rows, banks, and complete devices have structural and behavioral
  implementations. Nested bank, row, and cell fidelity overrides are supported.
- Structural devices and pin-expanded banks are limited to 4096 cells. Complete
  behavioral devices support up to 2^24 words, subject to available host memory.
- Python factories and the native/WASM visualizer use the same registered
  contracts. The complete behavioral device supports memory inspection/history
  and setup image loading.

This is not transistor simulation: the cell uses a NAND feedback latch, while
bitline charge, strong write excitation, sense validity, and access phases are
explicit digital abstractions. No analog voltage, calibrated device timing,
power consumption, or SRAM stability claims are made. The separate data buses
are simulator ports, not a pin-for-pin bidirectional-DQ package model.

The existing `MemoryBusAdapter32` does not directly connect to this SRAM port.
A technology-specific controller, synchronous SRAM wrapper, caches, DRAM, and
flash are outside this change.

## Validation

- SRAM implementation baseline: **226/226 tests passed**, 269.22 seconds wall time.
  Results: `build/sram-ctest-final.log`. The expected catalog inventory was
  extended to cover the new SRAM families, retaining exact inventory checks.
- `SRAMContractTest`: widths 8/16/32; geometries (N,K) = (1,1), (4,2),
  (6,2), (8,4); all byte masks; structural/behavioral and nested-fidelity
  comparisons; invalid configuration rejection; seeded randomized masked writes
  with an independently maintained expected-memory model.
- `SRAMTimingTest`: canceled writes before row activation, no early read result,
  superseded access timers, completed writes, and eventual queue quiescence.
- Python binding tests: exact ports, geometry and cell counts, fidelity
  selection, initialization/history, and bounds rejection.
- Native/WASM parity: 19 comparisons covering topology, checkpoints, example
  rows, sampled history, and mixed fidelity.
- Section 3.2 browser checks: all 12 embeds and all 115 checkpoint rows,
  structural/behavioral switching, mobile widths, no-JavaScript reference
  tables, and no backend API requests. Checked both the local preview and the
  generated static site.
- Existing section 3.1 browser regression: 7 embeds, 263 checkpoint rows.
- Documentation tests compile and execute the simplified SRAM C++ examples
  and check component tables against registered implementation availability.

Reproduction commands, after building native and WASM artifacts:

```bash
ctest --test-dir build --output-on-failure --parallel 4
node tests/SRAMWasmParityTest.mjs build-wasm/circuitsim_wasm.js build-wasm/circuitsim_wasm.wasm build/tests/VisualizerSessionProbe
python3 tests/SRAMDocumentationBrowserTest.py http://127.0.0.1:8878/
python3 tests/MemoryDocumentationBrowserTest.py http://127.0.0.1:8878/
build/tests/SRAMBenchmark
```

The browser commands require a server for `build-wasm/site`, Playwright, and
Chrome/Chromium. Build the published site using `tools/build-pages.sh` with
Emscripten and the documentation dependencies activated.

## Performance observations

`SRAMBenchmark` runs the same 32-bit, 8-word, 2-words-per-row workload with
93 checkpoints under three configurations. It checks all checkpoint snapshots
against the structural result and takes the median of three construction plus
simulation runs, including history recording.

| Configuration | Components | Processed events |
| --- | ---: | ---: |
| Fully structural | 2446 | 497964 |
| Behavioral bank, structural peripherals | 1098 | 247643 |
| Complete behavioral device | 1 | 1440 |

The complete behavioral device is substantially faster on this workload.
The compact bank reduces events but remains slower than the structural bank
in the measured small-device workload: it scans the expanded bank interface
on each evaluation. It is a tested fidelity option, not a guaranteed speedup.
Wall-clock measurements are host-load dependent; see
`build/sram-benchmark-final.jsonl` for the local measurements. None of these
host execution times describe physical SRAM access latency.

## Row/column visualization

The SRAM-specific layout keeps cells in numeric column order, stacks rows in
numeric row order, and aligns each bitline block with its column in every row.
The complete device places the row decoder to the left, precharge above, and
column selection, writing, and sensing below. Only presentation changes;
component ownership, pins, circuit wires, and simulator timing are unchanged.

`tools/generate-sram-layouts.py --reflow` explicitly refreshes these SRAM
layouts; without that flag it only supplies missing defaults. Previous default
geometry is retained for browser migration. Unchanged old defaults update,
while custom positions/colors and unrelated component layouts are preserved.

Layout follow-up validation: five focused CTest checks passed, including the
new geometry and migration tests and the existing profile/session/Rewire
regressions. Browser tests verified the actual cell/bitline alignment, saved
layout upgrades and subsequent edits, and all 12 section 3.2 embeds with their
115 checkpoints. Reflow is idempotent, and non-SRAM saved entries are unchanged.
