# Advanced pipeline implementation work

Scope: extend the existing `RV32IFiveStageCore`; preserve the single-cycle and
baseline cores and their public packet contracts. No caches or ISA extensions.

## Architecture decisions

- Reuse forwarding, load-use detection, memory requesters, and combinational
  Writeback. Refactor Execute around the tested shared execution datapath.
- Group prediction in a separate path: predecode, direction predictor, saved
  IF/ID and ID/EX next-PC values, and recovery. The saved values share the real
  packet enables; they are not additional pipeline stages.
- Five construction-time policies: not-taken, taken, backward-taken/forward-
  not-taken, one-bit, two-bit. Policy and table size are independent of fidelity.
- Direct JAL targets can be predicted; JALR resolves in Execute. No BTB/RAS.
- Compare full predicted and actual next PC. Train once when an eligible
  conditional branch leaves Execute; suppress updates on holds, bubbles,
  reset, or older terminal/fault suppression.
- Small FF-based counter entries and table first, then behavioral equivalents.
- Default zero-wait FF instruction/data endpoints; retain delayed endpoints.
- Keep metrics in host observation. Do not add architectural or trace storage.

## Work sequence

1. Counter, predecode, predictor, and recovery contracts plus reusable scenarios.
2. Grouped prediction path, core integration, and compact cycle-model parity.
3. Execute grouping/ALU sharing; focused program and control regressions.
4. Incremental docs: forwarding, load-use hazards, prediction, recovery,
   memory waits, complete core. Follow the existing IO/behavior/C++/
   implementations/examples/tests pattern.
5. Stage-ordered layouts, native/WASM/UI exposure, documentation build and
   bounded tests. Build and test serially to avoid OOM.

## Completed implementation

- Five configurable policies are registered independently of fidelity. Dynamic
  tables have 1..256 power-of-two entries, with all-binary structural/behavioral
  scenarios at 1, 4 and 16 entries. The one-bit policy deliberately reuses the
  two-cell row with duplicate bits; it is not an area-minimal one-cell design.
- PredictionPath groups predecode, prediction, two packet-aligned next-PC
  registers, recovery and correction selection. Recovery training is qualified
  by accepted EX advancement and older-instruction kill signals.
- Execute groups forwarded operands, the existing single-cycle datapath,
  next-PC selection and packet status. JAL/JALR use the shared ALU; the previous
  redundant control-flow target-adder assembly is no longer used by this stage.
- The standalone behavioral Execute now also respects a younger unready writer
  hiding an older WB value; a focused branch-operand regression covers it.
- Host observations count resolved branches, next-PC misses and corrections.
  No hardware trace buffer or performance-counter register was added.
- 25 policy/program scenarios cover direct/indirect calls (05), precise illegal
  stopping (09), load-use pairs (19), branch-heavy loop (20), and array sum (21).
- Section 4.6 has six incremental reference pages, binary C++ examples,
  complete preview checkpoint tables, embedded simulations and a theme-aware
  static diagram. The existing 4.4/4.5 core implementations remain unchanged.
- Saved layouts retain stage order, group prediction below the datapath, and
  arrange prediction-table entries in numerical rows. Manual layouts are
  skipped. Static and dynamic predictor helper placements coexist in the
  shared type layout, so switching policies does not lose helper geometry.

## Validation (2026-09-23)

All builds used one compiler job and CTest used `-j1`; no full test suite or
large benchmark campaign was launched.

- 51 integration/example tests passed, including all 22 default advanced-core
  programs, 25 configured predictor/program tests, exact documentation C++,
  grouped Execute, a single-cycle regression, and memory migration.
- 16 additional component/baseline tests passed: five prediction components,
  forwarding, load-use, coordinator, advanced assembly, and seven baseline
  blocks. Predictor/program tests compare retirement timestamps and branch
  observations across fidelities as well as architectural oracle lockstep.
- Memory migration includes all five policies with paused delayed ROM/SRAM,
  both expanded and fused endpoints, both core fidelities, wrong-path store
  suppression, and a backward branch loop. Requester checks cover all five
  memory technologies, request/response backpressure, reset and obsolete/ABA
  fetch responses.
- Documentation checks verify navigation, actual root IO names, checkpoint
  exports and saved placements. Seven diagram checks passed. Strict MkDocs and
  GitHub Pages packaging passed; focused example regeneration preserves the
  previous published bundle after WASM post-link repackaging.
- 22 native/WASM comparisons passed (six component scenarios plus five policy
  program scenarios, each in both fidelities), including hierarchy, pins,
  wires, full timeline/checkpoints, metrics, examples and endpoint inspectors.
- Chromium checks passed on all seven live 4.6 pages and three actual WASM
  scenarios. Verified light/dark diagram rendering, stage order, group
  placement, non-overlap, numeric table rows, and no backend API requests.
  Screenshots were inspected in `/tmp/rv32i-advanced-preview`.

## Small sanity measurement, not the planned performance study

Program 20 retires 51 instructions and resolves 16 conditional branches. Default
zero-wait FF endpoints and a 16-entry cold-reset predictor were used. Both core
fidelities agree on the simulated cycle counts; these are not wall-clock speed
or physical-frequency measurements.

| Policy | Cycles | Conditional next-PC misses |
| --- | ---: | ---: |
| not-taken | 85 | 15 |
| taken | 58 | 1 |
| backward-taken | 58 | 1 |
| one-bit | 60 | 2 |
| two-bit | 62 | 3 |

This loop favors taken prediction; two-bit starts strongly not-taken and needs
two taken outcomes to switch. It does not establish a general policy ranking.
Caches, BTB/RAS, speculative memory effects, and a comprehensive performance
campaign remain outside this change.
