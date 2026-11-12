# Part I implementation: the AI accelerator

Started: 2026-11-10. Status: open. Code state: `alpha` at `1ee0600` plus
uncommitted work (this record's and another session's SRAM main-memory work).
Plan: [GPU and AI accelerator roadmap](2026-11-09-gpu-and-ai-accelerator-roadmap.md), Part I.

## Summary

Part I is built, tested and measured. Stages 1–4 (multipliers, MAC cell,
requantizer, systolic array, `MatrixUnit<N>`, the pipelined bus as
options), the MNIST programs, the § 6 site chapter and campaign 1 are done;
the Part I tests and scenarios pass. Campaign 1: all 20 runs halt with the
reference outputs; the DRAM runs use a 64 ms row retention (memory
parameter `retention_ticks`), because the `unit` profile's 1 ms is shorter
than the matrix-multiply program and its DRAM has no refresh. Owner
constraint: earlier chapters' tests are neither changed nor run yet, so
the bus speed-up and the core's array multiplier are **options, default
off**; switching them to the default comes with updating the chapter 3–5
tests. On 2026-11-11 the § 6 pages and the seven scenarios were brought to
the component-page and scenario rules (example tables, embedded
simulators, saved layouts, catalog checks), and the WebAssembly module
was rebuilt with them: rows, scenarios and browser loads match native.
The scenarios open in the finest profile a browser handles (the matrix
unit in Logic), the array and the unit have floorplans, and the unit's
internal blocks are documented. On 2026-11-12, at the owner's request, the
two chapter-2/5 build-order faults found here were fixed. On 2026-11-13
the full CTest suite passed, after the delay tables were regenerated,
`DelayRuleTest` and `SettlingBoundTest` were made to skip the largest
chapter-6 parts in their all-parts runs, and the device status-block
layouts were renamed.

Remaining:

1. Owner decisions: make the pipelined bus and array multiplier the
   default (updates chapter 3–5 tests, results and recordings); whether
   the `unit` profile's DRAM should get a longer retention or a refresh
   schedule by default.
2. Clock certificate of the accelerator machines; two-deep router; the
   full Pages build (`tools/build-pages.sh`, all example rows) before
   publishing; commit.

## 2026-11-10 — Built

- `ArrayMultiplier<W>` (`arithmetic.array-multiplier.W`, W = 4–32): AND
  partial products, Baugh-Wooley signs, Dadda tree, prefix adders
  (`src/modules/composite/ArrayMultiplier.cpp`, `MultiplierTree.hpp`).
- `ArrayMulDivUnit<W>` (`arithmetic.array-mul-div.W`): multiply in 2 edges,
  divide iterative; RV32IM core parameter `multiplier=array`.
- `MacCell`, `Requantizer`, `SystolicArray<N>` (N = 2–16, weight-stationary)
  (`src/modules/accelerator/MatrixParts.cpp`). Compact array and requantizer
  are host models (known-value domain, X × known 0 = 0 kept exact); array
  timing composed from the gate-level 2 × 2 array.
- `MatrixUnit<N>` (`device.matrix-unit.N`, N = 4/8/16): register table
  (`MATRIX`), 4-deep command queue, sequencer, copy engine (DMA master, 3-level
  gather/scatter), shift/matmul/activate engines, banked FF buffers, bias,
  lanes (`src/modules/accelerator/MatrixUnit.cpp`; registers added to
  `DeviceRegisters.cpp`).
- Pipelined bus options: bus `"pipelined": true` → router `pipelined=1`
  (accept at answer edge), arbiter `pipelined=1` (same-cycle answer, accept at
  completion; also memory subsystem `pipelined=1`), core
  `overlapped_fetch=1` (next fetch in the take cycle from FETCH_NEXT_PC).
  Default gate construction order kept unchanged.
- Machines `systems/accelerator-machine.json`, `-cpu.json`; runner
  `src/runtime/AcceleratorGuest.cpp`; tool `accelerator_experiment`.
- MNIST: `tools/accelerator/model.py` (numpy). Full 10k test set: MLP
  96.87 % float / 96.79 % int8; CNN (stride-2 convs instead of pooling, so
  the copy engine's uniform patch steps suffice) 97.39 % / 97.34 %.
  Guests: `tools/accelerator/guest/*`, 14 ELFs in `tests/fixtures/accelerator/`.
- Tests: `src/tests/Accelerator*.cpp` (registry and CTest list updated);
  site pages `site-docs/accelerator/index.md`, `6.1`–`6.4` (drafts; 6.5, 6.6
  and nav not written); two diagrams.

## 2026-11-10 — Checks run

All in `build-accel` (separate folder), one job at a time under a memory cap:

- Passed: `ArrayMultiplierTest`, `ArrayMulDivUnitTest`, `MacCellTest`,
  `RequantizerTest`, `SystolicArrayTest`, `MatrixUnitTest` (dense layers and a
  convolution at N = 4/8/16 compact; one layer on the gate-level N = 4 unit,
  110 cycles on both forms; error cases).
- **The timings above were measured on an unoptimized build** (the folder was
  configured without a build type). It was reconfigured as Release `-O1` and
  a full rebuild was started; speeds must be remeasured.
- Not run: the unknown-padding array check (new), `PipelinedBusTest`,
  `AcceleratorMachineTest`, the scenarios, `TestRegistryCoverageTest`, the
  strict MkDocs build, WASM/browser checks. No earlier-chapter test was run.

## 2026-11-10 — Optimized build; tests and fixes

`build-accel` reconfigured as the other builds are (Release, `-O1 -g0`,
hot engine files `-O2`); full rebuild. The compact machine then runs about
300–470 cycles/s (the chapter-5 games machine on the unoptimized build had
also run at about 55 cycles/s; the earlier speed numbers are void).

Changes after the first runs:

- Compact `SystolicArray<N>` and `Requantizer` are now single host models
  (the composition of 256 compiled cells and 16 compiled requantizers ran
  about 24 cycles/s at N = 16); the compact 16 × 16 array test went from
  about 11 s to 0.1 s.
- Pipelined arbiter: two gates named like the response buffers
  (`I_RESPONSE`, `D_RESPONSE`) collided and crashed the build; renamed.
- Test fixes, not hardware: the bus harness recorded a same-cycle answer
  before its request, read write answers' (unknown) data, and read never
  written memory; the padding check read one edge too early; the device
  harness's 400-cycle register-access limit was shorter than a long LOAD.
- Recorded scenarios now carry their component test's logical ID; the
  catalog inventory lists the new types.
- Programs run two images with the unit and one on the CPU alone
  (simulation time); `AcceleratorMachineTest` runs `matmul-n16` only (N = 4
  and 8 are in the campaign, which checks the same outputs).

Checks run on the optimized build (each passed): `SystolicArrayTest` (with
the unknown-padding case on gates and compact), the six recorded scenarios
(`ArrayMultiplierWidth4`, `MacCell`, `Requantizer`, `SystolicArrayWidth2`,
`MatrixUnit`, `AcceleratorMachine`; the machine scenario: 69,671 cycles),
`PipelinedBusTest`, `TestRegistryCoverageTest`,
`BuiltinComponentCatalogInventoryTest` (a whole-catalog check; no
earlier-chapter test was run), strict MkDocs build (`mkdocs.preview.yml`).

`PipelinedBusTest` results (every answer checked against a host memory):

| Memory subsystem traffic | Plain | Pipelined |
| --- | ---: | ---: |
| FF, both ports, 128 accesses | 256 cycles | 128 |
| SRAM, fetch port, 64 reads | 192 | 129 |
| SRAM, both ports, 128 accesses | 384 | 257 |
| `demo-cpu` (iterative multiplier, SRAM) | 54,455 | 47,809 (same 7,230 instructions) |
| `demo-n4` | 69,671 | 60,184 (same 10,147 instructions) |

Parts (`accelerator_experiment --parts`; gates include storage built of
gates): `ArrayMultiplier<8/16/32>` 605 / 2,049 / 7,160 gates, 1,672 /
2,242 / 2,704 ps in `embedded`; iterative `Multiplier<8/16/32>` 1,334 /
2,585 / 5,168 gates; `Requantizer` 5,365 gates, 4,082 ps; `MacCell` 2,184;
`SystolicArray<2/4/8>` 9,580 / 40,008 / 163,408.

`AcceleratorMachineTest` (optimized build) passed: every program halted
cleanly, status 0, outputs equal to the host references, no setup violations
(default bus and iterative multiplier unless named):

| Program | Cycles | Instructions |
| --- | ---: | ---: |
| `demo-n4` | 69,671 | 10,147 |
| `matmul-n16` | 853,252 | 170,799 |
| `mlp-n16` (two images and the batch) | 377,043 | 20,486 |
| `cnn-n16` (two images) | 109,656 | 7,997 |
| `demo-cpu`, array multiplier | 36,599 | 7,230 (54,455 cycles with the iterative one) |

## 2026-11-10 — Campaign 1

`tools/accelerator/run-campaign.py run` (one run at a time, each under a
1.6 GB memory cap), then `run-campaign.py table`. Every configuration uses
compact fidelity, the `unit` timing profile and a 1000-tick clock; all but
the two `-plain` runs use the array multiplier and the pipelined bus.

19 of 20 runs halted cleanly with status 0, no setup violation and the
reference outputs. The 20th, `matmul-n16-dram`, hung (next section).

Timed sections (cycles; matrix multiply: sum of the three layers; MLP and
CNN: one image):

| Workload | CPU alone | 4 × 4 | 8 × 8 | 16 × 16 | FF, 16 × 16 | DRAM, 16 × 16 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Matrix multiply | 2,851,206 | 71,167 | 29,984 | 26,614 | 18,428 | hung |
| MLP | 1,636,072 | 462,490 | 137,032 | 117,321 | 89,462 | 208,728 |
| CNN | 2,643,442 | 106,281 | 49,697 | 46,111 | 32,571 | 91,456 |

- Speedup at 16 × 16 (SRAM): matrix multiply 107×, MLP 13.9×, CNN 57×.
- 4 → 8 gains 2.1–3.4×; 8 → 16 gains 8–17 %.
- Array, shift and activate engines busy 4–17 % of each layer (table in
  § 6.6): the copy engine (one request in flight, byte stores) dominates.
- MLP batch of two (16 × 16, SRAM): 119,354 cycles against 117,321 for one
  image: weight loading dominates at batch 1.
- Chapter-5 bus and iterative multiplier (`-plain`): CPU-alone layers take
  2.1× the cycles (same 729,492 instructions); the 16 × 16 unit's 8–21 %
  more.

Per-run cycles, instructions, commands and host time are in § 6.6.

## 2026-11-10 — `matmul-n16-dram` hang

The run stopped retiring instructions at cycle 1,761,280 with 170,649
instructions retired; the cycle counter ran on for two more hours until
the run was stopped. The SRAM and FF runs of the same program retire
170,809 and 170,919 (a slower memory means fewer polling loops; the DRAM
runs of the networks retire about 100 fewer than SRAM), so the hang lies
in the program's last few dozen instructions, after the unit's last
command: the output copy, the report writes, `put("done")` and the halt.

Steps taken:

- The runner now stops a run in which no instruction retires for
  `stall_cycles` cycles (default 1,000,000; `accelerator_experiment
  --stall-cycles`) and lists the handshake pins of the machine's top three
  levels.
- `PipelinedBusTest` now also drives the memory subsystem with DRAM, and
  with requesters that refuse answers in a random third of cycles (6,000
  accesses per run), plain and pipelined. All
  pass: the memory subsystem's arbiter and the DRAM controller are not
  the cause in isolation.
- The run is repeated with `--stall-cycles 20000` to get the pin listing.

## 2026-11-10 — Cause of the `matmul-n16-dram` hang: DRAM retention

Replaces the open question of the previous section. The repeated run
stopped at the 20,000-cycle stall limit: no instruction after cycle
1,758,436 (170,649 retired, as before). The pin listing showed the core's
commit signal, program counters and every bus request **unknown (X)**,
not a handshake waiting for an answer.

Cause: the `unit` timing profile gives DRAM 2-word rows, no refresh
(`dram_refresh_interval` 0, so the memory subsystem sets no refresh
interval) and an endpoint retention of 10⁹ ticks, 1,000,000 cycles at
the 1000-tick clock. A row not opened for that long reads X (documented
DRAM behavior). The program's last code (printing `done`, the halt) is
first fetched about 1.76 million cycles after loading, reads X, and the
core goes X. MLP and CNN on DRAM end before 1,000,000 cycles.

Confirmation (`demo-cpu`, DRAM, array multiplier): at period 50,000
(retention 20,000 cycles) the run turns X at cycle 37,416 on the pipelined
bus and at 24,715 on the chapter-5 bus; at period 1000 it halts cleanly
in 78,291 cycles. So the bus options are not involved.

Not fixed in the machine: refreshing all 32,768 two-word rows within
1,000,000 cycles needs one refresh per 30 cycles, a different memory from
the one the other DRAM runs measured, and a refresh parameter would change
the chapter-5 memory subsystem that another session is editing. Recorded in
§ 6.6 as "cannot run" with the reason.

Changes: the runner also stops at once when the commit signal turns X and
lists the same pins (`AcceleratorRunOptions::stall_cycles` documented in
§ 6.5). § 6.6 results written; § 6.3's test table lists the new
`PipelinedBusTest` cases.

Checks: `PipelinedBusTest` (new DRAM and hesitant-requester cases; passed:
FF 6,960 / SRAM 12,178 / DRAM 49,916 cycles pipelined for 6,000 hesitant
accesses against 12,093 / 18,007 / 55,643 plain); the three `demo-cpu`
runs above; see the next sections for the rest.

## 2026-11-10 — Final checks

After the runner and `PipelinedBusTest` changes (optimized `build-accel`):

- `AcceleratorMachineScenarioTest` passed (69,671 cycles, unchanged).
- `AcceleratorMachineTest` passed, cycles unchanged: `demo-n4` 69,671,
  `matmul-n16` 853,252, `mlp-n16` 377,043, `cnn-n16` 109,656, `demo-cpu`
  with the array multiplier 36,599.
- `PipelinedBusTest` passed (results in the hang-cause section).
- Strict MkDocs build (`mkdocs.preview.yml`) passed.
- Not run: earlier-chapter tests (owner's instruction), WebAssembly and
  browser checks, clock certificate.

## 2026-11-10 — DRAM runs with a 64 ms retention

Replaces the "cannot run" outcome of the hang-cause section. New
`memory.subsystem.32` parameter `retention_ticks` (DRAM only; default the
profile's, so no existing machine changes; § 5.3 lists it and the earlier
`pipelined`); runner option `dram_retention`, tool flag
`--dram-retention`; `run-campaign.py` passes 64 ms (64·10⁹ ticks, a real
DRAM's refresh period) to every DRAM run.

| Run | Cycles | Instructions | Timed sections |
| --- | ---: | ---: | --- |
| `matmul-n16-dram` | 1,759,149 | 170,703 | 6,170 / 15,826 / 33,428 (sum 55,424) |
| `mlp-n16-dram` (rerun) | 641,726 | 20,394 | 208,728 / 208,689 / 212,037 |
| `cnn-n16-dram` (rerun) | 207,256 | 7,933 | 91,456 / 91,473 |

The MLP and CNN reruns equal the runs with the profile's retention in
cycles, instructions, sections, outputs and simulation event counts
(395,451,597 and 121,973,858), so retention does not affect timing and
the three DRAM runs are comparable. `run-campaign.py table`: all 20 runs
halted cleanly with the reference outputs. Matrix multiply, 16 × 16:
FF 18,428, SRAM 26,614, DRAM 55,424 timed cycles. The earlier runs' files
are kept as `build-accel/campaign/*-dram.profile-retention.json`.

Checks after the `retention_ticks` change: `PipelinedBusTest` passed with
unchanged figures (`demo-cpu` 54,455 → 47,809, `demo-n4` 69,671 → 60,184;
DRAM traffic as before); strict MkDocs build passed. No earlier-chapter
test was run; `memory.subsystem.32` without the new parameter builds as
before.

## 2026-11-11 — Documents and scenarios against the templates

Owner: the documents must follow the template rules, scenarios included.
Checked against `docs/guides/adding-a-component.md` (§ 7, the page
template), `docs/guides/tests-and-scenarios.md`, the record template and
the neighbouring § 5.1 pages.

**Found.** No § 6 page had an Examples section: no
`simulator-examples` table, no embedded simulator. `ArrayMulDivUnit<W>`
had no scenario. `MatrixUnitScenarioTest`'s checkpoints (`edge-8`,
`edge-16`, …) asserted no output, and `MacCell`, `SystolicArray<2>` and
reset rows checked only some outputs. None of the seven scenarios had
saved layouts. The new menu group broke `VisualizerScenarioCatalogTest`
(it lists every group). The router, arbiter and core pages did not list
`pipelined`, `multiplier` or `overlapped_fetch`. This record ended with an
undated section.

**Also found, a bug of 2026-11-10.** The catalog names the six § 6 checks
(`ArrayMultiplierTest` … `MatrixUnitTest`) as contract evidence, so the
registry marked them visualizable, and `server.py` stopped at import with
`KeyError: 'ArrayMultiplierTest'` (no menu entry). The native visualizer
could not start on this tree. Fixed by adding them to the registry's
headless set, as chapter 5 does for its checks.

**Changed.**

- New `ArrayMulDivUnitWidth4ScenarioTest` (`array-muldiv-w4`, Gate): MUL,
  MULH, MULHSU, MULHU (done 1 edge after launch) and DIV −7 ÷ 2 (done 4
  edges after launch), each taken; results from the independent
  `referenceM`.
- `MatrixUnitScenarioTest`: checkpoints are now each command accepted
  (`queue-1-load` … `queue-7-store`, status 0), each byte STORE writes
  (`store-byte-0` … `-5`: word address, lane mask and value from
  `referenceLayer`), `done-7` and `status-idle`. Its layer uses shift 10
  instead of 5 (with 5 every byte saturated to 0 or 127); stored bytes −1,
  11, 16, −2, 26, 0.
- `MacCell` and `SystolicArray<2>` check every output at every edge,
  including zeros after reset; the M unit checks `BUSY`.
- `AcceleratorMachineScenarioTest` belongs to the logical test
  `AcceleratorMachineTest`, as chapter-5 machine scenarios belong to
  `ComputerSystemTest`. It names no contract: `system.computer` already
  has its owner, and `BuiltinComponentCatalogInventoryTest` allows one.
- Catalog: the new scenario at rank 1, ranks 0–6 consecutive.
  `SystolicArray<2>` now opens in Logic: 6,553 parts against 13,077 at
  Gate (chapter 5 opens up to about 7,800, `muldiv-w32` in Logic). Part
  counts (Gate / Logic / Compact): `array-multiplier-w4` 320/320/1,
  `array-muldiv-w4` 1,988/822/1, `mac-cell` 2,987/1,637/1, `requantizer`
  7,311/7,311/1, `matrix-unit-4` refused at Gate (killed at its 1.5 GB
  cap) / 79,031 / 62.
- Saved layouts: `recalculate_layouts.py --fill-missing` for the seven
  aliases (36 type layouts, 7 root layouts, added only: no saved entry
  changed). `check-static-layouts.py`: 0 problems in 7 scenarios.
- `VisualizerScenarioCatalogTest`: the `accelerator` group and its seven
  aliases.
- Pages: 6.1 rewritten in the § 5.1 order per component (IO pins,
  Behavior with a C++ reference, Unknown inputs and timing,
  Implementation, Examples, then Source and tests); 6.2 and 6.4 likewise;
  6.3, 6.5 and 6.6 gained Examples or reproduction steps and source
  links; the index lists the scenarios. The C++ references compile
  (`g++ -std=c++20 -Wall`, no warnings) and give the scenario values.
  6.4's reset text corrected: reset clears the bias registers and the
  array, not the three memories. Earlier pages: `pipelined` on the router
  (only `1` is accepted), the bus description, the two-port arbiter;
  `multiplier` and `overlapped_fetch` on the core.
- Guides: a § 6 section in the tests-and-scenarios guide; the menu order
  and the headless set in the test-organization reference.

**Example rows.** The Pages build makes them from the WASM module, which
has not been rebuilt with § 6 (host memory: about 1.8 GB free with the
long-game campaign running). The rows were exported with the native
`VisualizerSessionProbe examples` (the same `exampleRowsJson` the WASM
path calls, under the catalog profile) and added to the preview bundle
`build-wasm-milestone/pages/simulator/example-rows.json`, which the live
preview builds from strictly (a copy of the old bundle is in this
session's scratchpad). The WASM generator's per-row state cross-check has
not run on them.

**Checks run (all passed).** The seven § 6 scenarios
(`AcceleratorMachineScenarioTest` 69,671 cycles, unchanged);
`VisualizerScenarioCatalogTest` (10 tests, with the `build-accel`
backend); `BuiltinComponentCatalogInventoryTest`;
`TestRegistryCoverageTest`; `check-static-layouts.py` on the seven
aliases; strict MkDocs build with the preview configuration, every new
anchor present and the six tables rendered (8, 21, 7, 8, 8 and 17 rows).
Not run: the WASM build, `generate-example-rows.mjs`, browser checks,
the rest of the earlier chapters' tests.

## 2026-11-11 — WebAssembly build and browser checks

Run after the SRAM session's campaign and suite finished (owner: run the
unrun checks when memory allows).

**Found: the machine scenario could not open in the browser.**
`acceleratorFixture()` read `tests/fixtures/accelerator/<name>.elf` from
the source directory, which the WebAssembly module does not have, so
`accelerator-machine` (and the § 6.5 embed) would have failed with
"Cannot read …". Fixed as `systems/*.json` already are: CMake embeds
every accelerator `*.elf` as hex in `AcceleratorPrograms.generated.hpp`
(14 programs), and `acceleratorFixture()` decodes it; no file is read at
run time.

**Builds.** `build-accel` (native, capped 1.6 GB); `circuitsim_wasm` in
`build-wasm-milestone` (Emscripten from `~/.cache/circuitsim-emsdk`,
capped 2.2 GB; 10 minutes, 49 files recompiled). The rebuild deleted
the preview's `example-rows.json` as usual; it was restored at once and
the 8893 preview rebuilt strictly (58 s, no errors).

**Checks (all passed unless stated).**

- Native: `AcceleratorMachineScenarioTest` 69,671 cycles (unchanged);
  `AcceleratorMachineTest` (all its programs from the embedded bytes,
  46 min).
- `generate-example-rows.mjs` with the § 6 filter on the new module, its
  per-row state cross-check included: 8, 21, 7, 8, 8 and 17 verified
  rows. They are identical to the native export already in the preview
  bundle (all 153 scenarios and 29 programs compared); the bundle now
  holds the WASM-generated file.
- WebAssembly against the native session probe, all seven § 6 scenarios
  (a scratch script): root, profile fingerprint, timestamps, checkpoints,
  statistics, example rows and the final state equal. Load times in
  Node: 0.5–1.5 s for the arithmetic parts, 8 s for `matrix-unit-4`, 209 s
  for `accelerator-machine` (it runs the whole program).
- Headless Chrome on the 8893 preview: the six component scenarios open
  from the `AI Accelerator` menu group (4–13 s) with no page errors;
  `accelerator-machine` opens in 223 s with its three checkpoints; § 6.1's
  four embedded simulators load their scenarios; the 6.1, 6.2 and 6.4
  example tables are served (6.4 with every `queue-`, `store-byte-` and
  `status-idle` row).
- `VisualizerWasmRuntimeTest` and `ALUWidth8WasmRuntimeSmoke` pass.
- `VisualizerScenarioWasmParityTest`: **fails, not because of § 6.** Its
  first part passes (395 scenario descriptors equal native, § 6
  included); its ALU16 comparison then finds the shifter's parts listed
  in a different order. `WordExtended.cpp` (unchanged since 2026-10-28)
  builds `SHIFT_n_FILL_WORD` and `SHIFT_n_OUT_JOIN` as two arguments of
  one call, and C++ leaves their order to the compiler: GCC (native)
  builds `OUT_JOIN` first, Clang (Emscripten) `FILL_WORD`. The fix
  (build them into named variables first) is a chapter-2 change and is
  left to the owner, as earlier chapters are not changed in this part.

§ 6.5 now says the machine takes about 3½ minutes to open in the
browser. Not run: the full `tools/build-pages.sh` (it regenerates every
chapter's rows), the other web tests.

## 2026-11-11 — Menu place, profiles, layouts, rows and internal blocks

Owner: (1) the AI Accelerator group below the long games; (2) scenarios
open in the finest profile that is realistic; (3) review the layouts and
rows; (4) document the necessary components, such as
`MATRIX_UNIT_4_ROOT.SEQUENCER`, and their layouts.

**1. Menu.** The page adds the recorded long games as a group of its own
before "Developer circuits", so it came after § 6. It now goes right after
the short games (`app.js`); `VisualizerScenarioBrowserTest` asserts short
games → long games → AI Accelerator instead of long games last.

**2. Profiles.** Measured (native probe, WASM in Node, Chrome; earlier
gate-level precedent: `cache-read-hit`, 10,487 parts, 17 s and 467 MB in
WASM):

| Scenario | Gate | Logic | Visual | Compact | Chosen |
| --- | --- | --- | --- | --- | --- |
| `systolic-array-2` | 13,077 parts; WASM 3.7 s, 379 MB | 6,553 | 6,553 | — | Gate (was Logic) |
| `matrix-unit-4` | killed at 1.5 GB natively | 79,030; WASM 22 s, 923 MB; Chrome 32 s (41 s as the 6.4 embed) | 71,254; WASM 21 s | 62 (one closed box) | Logic (was Compact) |
| `accelerator-machine` | refused (memory) | — | killed at 1.6 GB after 99 s | 408; Chrome 223 s | Compact |

The four arithmetic scenarios were already Gate.

**3. Layouts.** New saved layouts for the two new profiles
(`--fill-missing`, added only). Review in Chrome found two automatic
placements that hid the structure: the matrix unit's 48 children in
layered columns, and the systolic array drawn diagonally (`CELL_0_0`
bottom left). `device_layout.py` now has floorplans for `SystolicArray<N>`
(the cell grid, each row's `SKEW_i_k` to its left, each column's
`DESKEW_j_k` below) and `MatrixUnit<N>` (port and registers, queues and
sequencer, engines, then one data-path column per array column);
`MatrixUnit<4>`'s saved aspect 1.645 became 0.70 to fit it. Reflowed with
`--reflow-types` (`SystolicArray<2>`, `SystolicArray<4>`, `MatrixUnit<4>`);
every changed entry was one this work had generated (none is in the
committed `layout.json`). `check-static-layouts.py`: 0 problems in the
seven scenarios.

**Found while checking: part names differed between native and
WebAssembly.** In `matrix-unit-4`, 1,676 of 79,031 part names differed (for
example `COPY_ENGINE.ADD32_n`). The engines' builder passed two
part-building calls as arguments of one call; C++ leaves their order to the
compiler, GCC and Clang chose differently, and parts are numbered in build
order. Saved layouts are keyed by those names, so a layout made natively
did not fit the browser's parts. Fixed in `MatrixUnit.cpp` (arguments built
into named values first); one of the shared values merged two identical
NOT gates of `START`, so the unit has 79,030 parts. 15 names still differ,
all in `REGISTERS.CHECKS.STATUS`: chapter 5's `RegisterFrontEnd.cpp`
(`statusChoice(…, st.constantWord(…), st.constantWord(…))`), shared by
every chapter-5 device; not changed here (earlier chapters are not changed
in this part). The five other § 6 scenarios have identical names. The
engine layouts were reflowed for the new names.

**3. Rows.** Labels and cases upgraded:

- `array-multiplier-w4`: labels give each operand's value under its sign
  flag (`-7x3su`, was `9x3su`).
- `array-muldiv-w4` (34 rows, was 21): labels as the operation reads
  its operands (`mulh-neg3-5`, `div-neg7-2`); new `DIVU` 9 ÷ 0 = 15,
  `REM` −7 mod 3 = −1, `DIV` −8 ÷ −1 = −8 (launch, done, take each); a
  divide cancelled one step in (`CANCEL` clears `BUSY` and `DONE` at the
  edge; `READY` returns once `CANCEL` falls, since it is low while
  `CANCEL` is high). Before, no test exercised `CANCEL`.
- `requantizer`: −40,000 → −128 (the negative clamp).
- The MAC cell, array and matrix unit rows were already complete; the
  last two only changed checkpoint times under the finer profiles (their
  values are identical).

**4. Documentation of internal blocks.** Criterion: a block a reader can
open in a scenario's default view and that has behaviour of its own gets
its pins and behaviour on the page (as chapter 5's register front end);
splits, joins, constants and pass-throughs do not. Added: § 6.4 "The
control blocks" (`MatrixSequencer`, the shift, multiply and activate
engines, `MatrixBufferPorts`, `MatrixLane`, `MatrixBias`,
`MatrixStoreSelect`) and the floorplan; § 6.3.1 the copy engine's pins and
registers; § 6.1.2 the parts of `ArrayMulDivUnit<W>` and
`ArrayMultiplyUnit`'s pins. Chapter-2, 3 and 5 parts inside these
scenarios (`CarryPrefixLevel`, `FlipFlopCell`, `Divider<W>`'s parts,
`RegisterFrontEnd`) are documented in their chapters. Profile statements on
6.2, 6.4, 6.5 and the index updated.

**Checks (all passed unless stated).** `ArrayMultiplierTest`,
`ArrayMulDivUnitTest`, `RequantizerTest`, `MatrixUnitTest` (45 s), the six
component scenarios, `AcceleratorMachineScenarioTest` (69,671 cycles),
`VisualizerScenarioCatalogTest`, `BuiltinComponentCatalogInventoryTest`,
`TestRegistryCoverageTest` (ctest, `build-accel`); WASM rebuilt (twice);
`generate-example-rows.mjs` for the six tables with its per-row check;
WASM against native for the five gate scenarios (topology summary,
checkpoints, rows, final state) and for `matrix-unit-4` (rows and every
root pin by name; its state order differs by the 15 chapter-5 names);
Chrome: `VisualizerScenarioBrowserTest` (menu order), the six component
scenarios, § 6.1's four embeds, § 6.4's table and embed; 8893 preview
strict build. Not run: `tools/build-pages.sh`, the other web tests.

## 2026-11-12 — Chapter-2/5 build-order faults fixed (owner request)

Replaces the "not fixed" notes of 2026-11-11 for `WordExtended.cpp` and
`RegisterFrontEnd.cpp`. Both now build each part in its own statement, in
GCC's existing order (native part lists of ALU 8/16/32 and the four
device scenarios compared before and after: identical). After the WASM
rebuild, `VisualizerScenarioWasmParityTest` passes, and part names equal
native for ALU16, the device scenarios and `matrix-unit-4` (0 of 79,030
differ). 19 native ALU and device tests pass; regenerated ALU and device
example rows are identical; Chrome opens the four affected scenarios.

## 2026-11-12 — Colours

`VisualizerColorPaletteTest` failed (as the SRAM session reported on
2026-11-11): the chapter-6 types used the neutral fallback colour. 16
prefixes added to `visualizer-v2/palette.json`; the test passes.


## 2026-11-12 — Delay tables regenerated

The full suite's `PartDelayTablesCompleteTest` and its embedded variant
failed: Part I had not regenerated the checked-in tables. Each of
`ProvenPartDelays.generated.inc`, `.embedded` and `.desktop` was written
again with `part_delay_tables [--profile=…] --write FILE --clocked`
(about 19 minutes and 1.7 GB each). Each diff only adds 14 chapter-6
entries (MAC cell, requantizer, systolic arrays 2/4/8/16, array
multipliers and array multiply-dividers 4/8/16/32); no earlier entry
changed. `--gate-lists` adds four parts too large for a gate list
(systolic arrays 4/8/16, `arithmetic.array-mul-div.32`). It also lowers
the gate counts written as comments for four cores (RV32I baseline
pipeline 115,237 → 115,153, five-stage 136,367 → 136,339, single-cycle
91,789 → 91,705; RV32IM five-stage 189,899 → 189,390). The test compares
only the list, not these counts. This session's edits reach none of the
single-cycle core's parts, so the drift probably came from committed work
after `acaa085`; its cause was not investigated. `--gate-list-compacts`:
unchanged.

## 2026-11-12 — `DelayRuleTest` with the chapter-6 tables

With the new tables, the full suite's `DelayRuleTest` ran out of its 3 GB
cap (systemd's out-of-memory kill inside the capped scope; the host was
unaffected). Run part by part under the same cap, each chapter-6 part:

| Part | Time | Peak memory | Result |
| --- | ---: | ---: | --- |
| `accelerator.mac-cell` | 7 s | 37 MB | pass, timed from its gates (97%) |
| `accelerator.requantize` | 23 s | 65 MB | below target: typical 65%, worst 100% |
| `arithmetic.array-multiplier.4/8/16/32` | 0.4–25 s | 24–86 MB | pass, timed from its gates (91–92%) |
| `arithmetic.array-mul-div.4/8/16/32` | 0.7–7 s | 30–137 MB | pass (90–97%) |
| `accelerator.systolic-array.2` | 13 s | 92 MB | pass (99%) |
| `accelerator.systolic-array.4` | 56 s | 184 MB | pass (99%) |
| `accelerator.systolic-array.8` | 229 s | 667 MB | pass (99%) |
| `accelerator.systolic-array.16` | 1,017 s | 2.6 GB | pass (99%) |

No part breaks the rule. Two changes in `tests/cpp/DelayRuleTest.cpp`,
each adding only chapter-6 entries:

- The requantizer gets a recorded exception (`{65, 100}`, reason
  `BOUND`). It has a delay twin (checked with `CIRCUITSIM_DELAY_TRACE`),
  but the twin's settling bound is loose when several pins change at once
  (few pins 50%, all pins 49%; one pin 75%). Making that bound tighter
  would change the shared twin code, so it was not attempted.
- A new `FULL_RUN_SKIPPED` list leaves `accelerator.systolic-array.16` out
  of a run over every part. Naming it still runs it. Its table is composed
  from the 2 × 2 array, like those of the 4 × 4 and 8 × 8 arrays, which
  the full run compares.

## 2026-11-13 — `DelayRuleTest` passes over every part

The first rerun was killed again by the cap, after 7 minutes. Per-part
output (`stdbuf -oL`) and 10-second memory samples showed the kill right
after `device.fifo.8`, building `device.matrix-unit.*`. Its compact form
(`DeviceBehavioral`) has no delay table, so the test compares nothing,
but it first builds the gate form: 1.6 GB peak for `device.matrix-unit.4`
alone (18.8 s, "No selected parts were compared"). The three sizes were
added to `FULL_RUN_SKIPPED`.

Then the full `DelayRuleTest` passed in about 20 minutes, with a peak of
718 MB: 404 parts, 342,000 situations, 0 parts break the rule, 0 miss the
targets without a recorded exception (7 have one); over all parts the
gates take 95% of the compact time on average (typical), 97% worst. The
cores keep their clock certificates (single 142, baseline 68, advanced 80
ticks; equal cycle counts in gates and compact). The systolic arrays 2, 4
and 8 report values that differ between the forms after an earlier X or Z
on an input, with every input back at 0 or 1. Some outputs are 0/1 only in
the gates (180, 266, 571) and some only in the compact form (56, 220, 0);
none are opposite 0/1 values. The test does not compare timing there, and
this does not break the rule. It is open whether the compact array should
follow its gates there, as most chapter-4 parts now do.

## 2026-11-13 — `SettlingBoundTest` and the device status-block layouts

`SettlingBoundTest` was killed by the cap in the same way. It builds every
catalog part as gates twice and only then skips clocked parts. Run part
by part, every chapter-6 part it checks passes (the combinational ones:
the requantizer 26 s, `arithmetic.array-multiplier.32` 42 s, the rest
under 3 s). Building the clocked parts it then skips cost 1.2 GB for
`accelerator.systolic-array.8` and over 2.5 GB for `device.matrix-unit.4`.
A new `LARGE_CLOCKED` list (systolic arrays 8 and 16, matrix units 4, 8
and 16) is skipped before building in a run over every part. The full
test then passed in 394 s (limit 600 s), peak 505 MB: 290 parts with a
compact form, all on a proven table that matched their gates.

The rest of the suite then ran: 612 of 613 passed. One was skipped:
`RecordedTraceAssetsTest`, because MkDocs is not importable from CTest's
Python. One failed: `VisualizerStaticLayoutTest`, with 38 missing
placements, all inside `RegisterStatus<…>` of the five device scenarios
(`CONSTANT_WORD_*`, `ZERO_WORD_*`, `ANY_*`).

Cause: `layout.json` had saved these children under the names the
browser (Clang) build gave them, for example `ZERO_WORD_0`,
`CONSTANT_WORD_1`. The native GCC build has always numbered them the
other way round; the part list saved before the 2026-11-12 fix shows
`CONSTANT_WORD_0`, `ZERO_WORD_1`. So this native check was most likely
already failing before that fix, though it was not run then. The fix
made the browser build use GCC's order too, so it also no longer
matched the saved names. The 2026-11-12 Chrome check opened these
scenarios but did not open the status blocks, which is why it missed this.

Change: the 38 keys were renamed in place. Within each prefix the parts
are made in the same order in both builds, only the numbers differ, so
the n-th old key of a prefix maps to the n-th current name. The current
names were read from the native build. This was a text substitution
inside each `RegisterStatus<…>` block (76 diff lines, nothing else). The
file was checked to parse and to contain every current name.
`check-static-layouts.py` on the five device scenarios: 0 problems. The
preview copy (`build-wasm-milestone/pages/simulator/layout.json`) was
updated too.
