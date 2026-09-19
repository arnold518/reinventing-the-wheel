# Memory devices: DRAM, ROM, NOR flash

## Scope and boundaries

Keep the existing FF/SRAM implementations and their interfaces. Add three
independent device families, not new modes of MemoryContainer and not bus
controllers. Structural remains the default for small circuits. New terminal
digital primitives represent charge storage/sensing; none claims transistor,
voltage, power, JEDEC timing, or pin-for-pin package accuracy.

- DRAM: one bank, explicit ACT/READ/WRITE/PRECHARGE/REFRESH commands, open-row
  state, destructive cell sampling and restoration, configurable retention.
- ROM: immutable construction image, row/column selection, output enable.
- NOR flash: erased-one cells, one-way bit programming, block erase, command
  latching, busy/error/read-valid status. Not NAND and not SPI emulation.

Reuse construction wiring, catalog/fidelity selection, event scheduling,
MemoryStorage where suitable, scenario/checkpoint tests, native/WASM exposure,
and visualization. Do not impose identical cell or device pins. Shared matrix
decoding/selection helpers are circuitry, not a universal storage-cell model.

## Geometry and scale

External widths 8/16/32; runtime depth N and words per row K; rows=N/K and
columns=W*K. Small structural banks are capped at 4096 cells. Pin-expanded row
and bank interfaces remain bounded even for behavioral replacements. Compact
devices use packed storage and bounded geometry, never silently expand millions
of cells. NOR erase blocks contain an integral number of rows. DRAM exposes
row and column addresses through its command address, independently of W.

## Implementation sequence

1. Shared, tested matrix decoding/column selection and pin helpers.
2. Technology primitives/cells, rows, banks and peripheral circuits.
3. Small structural complete devices; checkpoint scenarios for each public
   component and explicit timing/invalid-input tests.
4. Compact cell/row/bank/device replacements where useful; independent expected
   values and cross-fidelity/mixed-fidelity comparisons.
5. Strict public factories, registry and native/WASM integration.
6. Matrix defaults and saved layouts: CELL_0..C-1 left-to-right, ROW_0..R-1
   top-to-bottom, column circuits aligned with their cells, decoder to the left,
   control and IO outside the bank. Preserve existing customized FF/SRAM layouts.
7. Move old 3.1 -> 3.1.1 and 3.2 -> 3.1.2; add architecture introduction 3.1,
   DRAM 3.1.3, ROM 3.1.4, NOR flash 3.1.5. Update nav, links and test paths.

## Validation and resource limits

Run only new-component tests (including their bindings, layouts, documentation,
and WASM parity), not the full repository suite. Compile with one job and
bounded process address space; run scenarios and browser pages sequentially.
Monitor available RAM and avoid starting builds when headroom is insufficient.
Keep default examples tiny and browser contexts short-lived. Reuse existing
documentation example data for unchanged scenarios rather than rerunning them.

DRAM retention is checked lazily on access, so an idle array does not create
periodic events or prevent a scenario from settling. Tests must include long
idle periods and refresh before/after expiry. Event timestamps and propagation
are fidelity-dependent; compare settled contracts, not identical internal waves.
No real-time performance or electrical-fidelity claim follows from these tests.
