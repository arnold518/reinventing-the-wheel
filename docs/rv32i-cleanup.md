# RV32I completion cleanup

This cleanup removes superseded implementations, not CPU functionality. The single-cycle, baseline pipeline, advanced pipeline, instruction oracle, technology-specific devices, structural slices, and native/WASM visualizers remain supported.

## Memory architecture

- `FFMemory<W>` remains the structural FF device; private `FFMemoryDirect<W>` is its compact behavioral implementation, with unchanged pins, edge/reset semantics, and delay.
- FF contract IDs `memory.container.ff.W`, `.behavioral`, and `.structural-slice` remain stable for saved profiles. Historical FF layout keys also remain stable.
- The old generic `LegacyMemoryContainer`, `MemoryArray`, `MemoryWord`, and `MemoryStorageBank` are removed. Their old latch-SRAM/constant-ROM/ideal-reference contracts are not aliases for the native technologies: those interfaces were different.
- The FF address decoder, access control, and read selector remain. `FFWord` and `FFStorageBank` remain. (`RegisterFile4x32` was retired later; see the October 7 section.)
- `MemoryBusAdapter32`, `Memory64Kx32`, `Memory4x32`, and `Memory32x32` are removed. Use a native device for direct accesses or the matching controller/endpoint for transactions.
- The collapsed instruction answer sheet owns `MemoryStorage`, not unconnected memory components. The oracle reads through a non-mutating host view; its owner applies stores with simulated timestamps. Real CPU ports remain transaction endpoints.
- `RV32IProgram::loadInto` accepts `MemoryStateView`, allowing image provisioning independently of device technology.

## Tests and visualizer

- `FFMemoryTest` / `FFMemoryContractTest` retain FF structural/behavioral, geometry, masking, reset, invalid-address and timing coverage. The old FF scenario URL `ff-memory-container` remains valid.
- `ScalableMemoryEndpointTest` names what it actually tests. Its existing URL also remains valid.
- Legacy-only memory/adapter scenarios are removed. Current device and endpoint tests already cover native retention, masks, read-only errors, alignment/range rejection, held responses and reset behavior.
- `MemoryArchitectureCleanupTest` guards retired catalog IDs, surviving FF IDs, evidence references, generic image loading, historical zero writes and the answer sheet's non-mutating reads.
- FF Python/browser/WASM tests are renamed and retain the saved-layout and fidelity-boundary checks. Technology-specific tests remain separate.
- Pygame scenario construction now uses the shared registry; it is not removed.
- CTest now includes all 22 baseline program scenarios and all
  110 policy/program prediction scenarios in registry coverage, in the same order as the shared
  C++ registry. The component inventory includes the five prediction families.
- Visualizer binding checks reflect the current prediction and fetch-discard
  circuitry. Missing semantic colors are restored, including inert legacy
  layout keys; no retired component is reintroduced.
- Balanced matcher reductions are laid out by tree level. Execute/prediction
  floorplans include their newer control gates and next-PC selection. The
  scoped `tools/refresh-rv32i-optimized-layouts.py` generator preserves manual
  scopes, unrelated layouts, styling and browser migration history.
- The instruction-reference documentation test checks the published §4.4 core
  pin table against the compiled universal memory-port schema. Supporting notes
  link to that table instead of retaining the obsolete native-memory interface.

Removed APIs deliberately fail instead of silently changing hardware semantics. User layout entries are not bulk-deleted: preserved keys allow existing FF layouts to continue working, while entries for absent components are inert.

## Benchmark and build hygiene

- `tools/benchmarks/run.py` is the common campaign entry point. `rv32i_benchmark_elf` remains a delegating compatibility CLI.
- The old `rv32i_performance` executable and one-off CRC32 launch script are retired. Diagnostic `CircuitTimingAnalyzer` remains distinct from sufficient-clock certification.
- `MemoryStorageBenchmark` replaces the misleading `MemoryContainerBenchmark` name, without changing its workload or result identifiers.
- `CIRCUITSIM_DEPLOY_PYTHON=OFF` allows isolated builds without overwriting the live preview module. CTest imports the selected build directory first.
- Compile and test serially on this host. Background benchmark snapshots, binaries, ELFs, logs and results are never cleanup targets.

## Validation

The planned cleanup validation is complete. Results below combine the unchanged
native/WASM artifacts with the resumed browser checks; this is the selected
cleanup suite, not a claim that every repository test or benchmark was rerun.

- Python syntax checks, JavaScript syntax checks and `git diff --check` passed.
- Twelve focused diagram, rendering, geometry and saved-profile migration
  checks passed. The profile migration subprocess required an unsandboxed
  rerun; the initial sandbox denial was not a logic failure.
- The isolated native build completed. Its first complete selected native run
  passed 80/86 checks; the six failures were catalog/registry inventory drift,
  old visualizer topology/layout expectations, missing palette entries and a
  stale documentation pin table. Those issues are fixed: all nine focused
  regression checks now pass, including the six previously failing checks.
  The scoped layout generator is idempotent and its tests cover one-bit,
  odd/even-width and full-width matcher trees, pin clearance and preservation.
- The full selected native rerun passed **90/90** checks. The storage benchmark
  smoke check, WASM/site build, all four native/WASM parity suites (FF memory,
  endpoints, inspector and RV32I memory), and WASM program-loader check passed.
  Evidence: `build/validation/cleanup-20260929-143407/`.
- Browser validation initially stopped before executing its assertions because
  `/home/arnold/venv/bin/python` lacked Playwright. Playwright 1.60.0 and Chrome
  were already available to `/usr/bin/python3`; no package installation or
  shared-environment modification was required.
- The runner now accepts `--browser-python` (or `CIRCUITSIM_BROWSER_PYTHON`),
  defaults to its own Python, and launches a browser preflight before costly
  builds. `--browser-only` checks the existing site without repeating native
  builds or claiming native/parity validation. Status files record the scope.
  `--browser-test NAME` can retry just a failed browser suite. The five runner
  regression checks cover interpreter choice, preflight failure, missing site
  artifacts, preservation of the full path and selective browser resumption.
- Four browser suites passed in `build/validation/cleanup-20260929-192038/`:
  FF-memory fidelity/layout persistence, rewire rendering, all 263 FF
  documentation checkpoints, and instruction/load/store highlighting with
  paging/scrolling/rewind across all three RV32I cores.
- The final visualizer smoke test exposed a stale §2.1 expectation: the test
  assumed three behavioral-capable components, omitting the structural-only
  `HalfAdder` and `FullAdder` added to the page. It now checks all five component
  identities, their exact implementation availability and all five embedded
  simulations, including their 80 four-state rows (729 Chapter 2 rows total).
  The camera regression also now follows ALU32's prefix-adder hierarchy instead
  of requiring its superseded full-adder ripple chain; the DOM-based centering
  and framing tolerances are unchanged. The final rerun passed in
  `build/validation/cleanup-20260929-193255/`: 729 documented example rows,
  30 camera-framing checks, six animated zoom/reset controls, ALU8/ALU32
  assembly layouts, and saved layout/profile persistence. All five planned
  browser suites have now passed across these resumed runs.

Local status/logs are the newest `build/validation/cleanup-*/status.json` and
the adjacent per-phase logs. The queue stops on failure; each retry preserves
the earlier attempt's evidence in its own directory.

The first persistent attempt hit GCC's 2 GiB virtual-address limit while
compiling Python bindings. The retry uses a 4 GiB address-space limit while
retaining a whole-job 2,500 MiB RAM / 512 MiB swap cgroup cap and serial builds.

No existing benchmark artifact is relabelled as a measurement of this revision.
The September 28 reboot removed the temporary validation runner/logs and
pre-cleanup archive under `/tmp/rv32i-cleanup-fyxb0g`. The replacement runner
is `tools/validate-cleanup.py`; its logs now live in the persistent build tree.
Generated Python cache files and the old tracked Pygame backend binary were
removed from the Git index only; their local copies were preserved.

## Scenario inventory follow-up (September 30)

The shared visualizer catalog now gives components stable groups and shows each
of the 22 fixed programs once, with separate core and prediction selectors.
All 110 prediction-policy/program combinations and all 22 baseline programs
are registered in CTest as well as the shared visualizer registry. Empty
verification-only tests remain in CTest but are not presented as circuits.
Standalone browser and WASM checks are inventoried in `tests/web-tests.json`
and run serially by `tools/run-web-tests.py` against one selected site artifact.

The first complete 36-check web inventory exposed seven failures: outdated
ALU32 and single-cycle test expectations, a manual-only saved-layout fixture,
and missing published ALU32, baseline-fetch, execution-status and prediction
program layouts. The affected checks were corrected or their scoped layouts
regenerated. Across the original run and targeted reruns, every one of the 36
checks has a passing result under `build/validation/web-20260930-scenario-cleanup*`.
The packaged-layout preflight now checks all 176 fixed-program roots, including
the prediction policies.

The final focused native run passed 8/8, benchmark-infrastructure checks passed
13/13, and all 112 prediction-labeled checks passed (including all 110 new
policy/program scenarios). The native and WASM builds succeeded, syntax checks
and `git diff --check` passed. These runs do not claim a complete repository
CTest run or a completed long Embench/middle-scale campaign.

## Unused component retirement (October 7)

Components that no other component instantiates and that no site-docs page
describes were retired together with their tests, catalog contracts, Python
bindings, CTest names and visualizer menu entries:

- `RV32IPipelineControlFlowUnit` (structural and behavioral). The advanced core
  resolves control flow in Execute and the prediction path; nothing built it.
  It is also removed from the five-stage core's representative-slice evidence.
- `Comparator32`. ALU32 derives comparisons through `ALU32CompareUnit`.
- `ShiftLeftLogical8`, `ShiftRightLogical8`, `ShiftRightArithmetic8`,
  `Comparator8`, `SignedComparator8`, `AND8`, `OR8`, `NAND8`, `NOR8`. ALU8 uses
  `BarrelShifter8`, `LogicUnit8` and `CompareSelect8`.
- `Mux16to1`, `Mux16to1_8bit`, `Mux2to1_8bit`, `Mux4to1_8bit`.
- `RegisterFile4x32` and `Decoder2to4`, which only it used.

Kept deliberately: `EqualityChecker8`, `NOT8` and `XOR8` (used by the section
2.1 arithmetic components), the section 2.1 components themselves, and the
documented standalone `RV32IControlFlowUnit`. Saved `layout.json` and
`palette.json` entries for the retired types are inert and were not deleted.
The scenario catalog now lists 169 components.

## One word for behavioral implementations (October 8)

Behavioral classes and their files were named `...Direct`; they are now named
`...Behavioral` (for example `ALU32Direct` became `ALU32Behavioral`), matching
the catalog's "structural" and "behavioral" terms. Only C++ names and file
names changed. Catalog implementation IDs such as `rv32i.alu32.behavioral.direct`
are stored in saved profiles and were left as they are. Earlier sections of
this log use the old names.

## Memory catalog names follow one rule (October 8)

Memory contracts are now named `memory.<technology>.<what>.<width>`:

| Before | Now |
| --- | --- |
| `memory.write-enabled-bit` | `memory.ff.cell` |
| `memory.ff-word.W` | `memory.ff.word.W` |
| `memory.ff-storage-bank.W` | `memory.ff.bank.W` |
| `memory.container.ff.W` | `memory.ff.device.W` |
| `memory.device.T.W` | `memory.T.scalable-device.W` |
| `memory.controller.T.W` | `memory.T.controller.W` |
| `memory.endpoint.T.W` | `memory.T.endpoint.W` |
| `memory.scalable.T.W` | `memory.T.scalable-endpoint.W` |
| `memory.matrix.*` | `memory.shared.*` |
| `memory.transaction-port.W` | `memory.shared.transaction-port.W` |
| `memory.response-buffer.W` | `memory.shared.response-buffer.W` |

The detailed `memory.<technology>.{cell,row,bank,device}` names are unchanged.
General storage parts left the `memory.` prefix in a second step:
`memory.register.W` is now `storage.register.W` and `memory.clocked-state-bit`
is `storage.state-bit` (a bare flip-flop with no write-enable, so it is not a
one-bit register). `circuit::canonicalContractId`
(`SelectionTypes.cpp`) maps every old name to its new one; catalog lookups,
build requests and profile rules all accept the old names.
`withBehavioralMemoryParts` still names the flip-flop cell, the state bit and
the registers by their old IDs on purpose, because saved layouts are keyed by that profile's fingerprint.
The `expanded_storage` endpoint parameter was removed at the same time:
request the `scalable-endpoint` contract instead.

## One wiring style, and inputs wired straight to outputs (October 8)

- The 27 component sources that created each wire by hand
  (`builder.addNewWire(name, source, sinks)`) now use
  `circuit::detail::StructuralWiring`: `connect(source, sinks)` then `finish()`.
  Their wires are named `net_<index>` and ordered by source pin. A
  name-independent net list of all 384 scenarios is identical for every scenario
  that contains no removed filler.
- `ComponentBuilder::addNewWire` used to drop, silently, a wire from a
  component's own input pin to its own output pin. It now creates it as an
  internal wire.
- The zero-delay `Rewire` fillers that existed only to work around that were
  removed from the RV32I stages: `VALID_PASS` and `NEXT_PC_BRIDGE` (fetch),
  `PASS` (decode), `PACKET_PASS` (execute), `PASS` and `DMEM_PORT_PASS` (memory),
  `VALID_PASS` (writeback), the baseline pipeline's `PACKET_PASS`, `STORE_DATA`
  and `WRITEBACK_FLUSH`, and `RESPONSE_DATA` in the memory request blocks.
  Tick timing is unchanged because the fillers had zero delay. Saved layout
  entries for them are inert and were not deleted.
- Still present: the memory-side bridges (`REQUEST_*` in the transaction port,
  `DEVICE_CLOCK`, `BUS_CLOCK`, the zero-wait port's bridges and the SRAM pass
  gates), which are drawn and documented as visible parts.

## Relay "container" technology retired (October 8)

The memory technology named `container` was a transaction-to-transaction relay
(`MemoryController<container,W>` and `MemoryEndpoint<container,W>`). It shared
its name with `MemoryContainer`, the scalable native device, and nothing used
it. It is removed together with its relay phases in the memory sequencer, the
`MemoryEndpointRelayTest` scenario, its section on the 3.3 page and its part of
the tested reference listing. "Container" now only means `MemoryContainer`.

## Documented but unused parts retired (October 8)

- The eight 8-bit helpers that only each other used: `TwosComplement8`,
  `Subtractor8`, `SubtractorWithBorrow8`, `Incrementer8`, `Decrementer8`,
  `EqualityChecker8`, `NOT8` and `XOR8`, with `Arithmetic8`, `Logic8` and
  `Comparator8` sources, their tests and the "existing arithmetic helpers"
  table on the 2.1 page. `ArithmeticUnit8` covers the same operations.
- `RV32IControlFlowUnit` (contract `rv32i.control-flow`) and its page, section
  4.3.5. No core used it; Fetch, Execute and Next PC do its work. Memory
  Access and Execution Control and Status became 4.3.5 and 4.3.6. The
  contract ID stays in the retired list in `RV32IBuildProfiles.cpp` so the
  preview profile's fingerprint does not change.

Saved `layout.json` and `palette.json` entries for these types are inert.

## Saved layouts for retired parts removed (October 8)

`tools/prune-retired-layouts.py` lists, and with `--apply` removes, `layout.json`
entries that nothing can reach any more: a type whose base name appears nowhere
in `src/` or `include/`, a part of the retired `container` relay technology, or
a root layout whose scenario alias belongs to no registered test. It removed 38
type entries, 4 profile entries and 38 root layouts (about 440 KB). This
replaces the earlier policy of keeping those entries; placements of live types
and scenarios are untouched, including child placements for removed filler
parts inside layouts that are still in use.

## Storage and memory are two families (October 8)

State in the circuits is one of two kinds. Storage has no address and is
always flip-flops: `Register<W>` (`storage.register.W`), `ClockedStateBit`
(`storage.state-bit`) and single `FlipFlopCell`s. Memory is reached by address
and comes in five technologies (`memory.<technology>.*`). The register file's
words are registers; its decoder and read selectors supply the addressing.

`Register` and `ClockedStateBit` sources moved from `modules/memory` to
`modules/storage`, and `ClockedStateBit` got its own files (it used to live
inside the memory controller's). Both are documented in section 4.3.1 of the
site, which also states the distinction; section 3.2 now points there for the
state bit. No circuit changed.

The five smallest parts (`SRLatch`/`GatedDLatch`, `DFlipFlop`, `FlipFlopCell`,
`HalfAdder`, `FullAdder`) used a third, fluent form
(`builder.wire(name).fromInput(...).to<T>(...)`). They now use
`StructuralWiring` like everything else, and the `WireBuilder` class is
removed. Their wires are named `net_<index>`.

## Behavioral forms added where a part blocked its parent (October 8)

`Adder32`, the four 32-bit bus muxes, `Decoder5to32` (and its register-file
variant with output 0 unused, contract `decode.5-to-32.x0-unused`),
`RV32IMemoryRequest` and `RV32IMemoryLanes` gained behavioral forms under
their existing contracts. The component tests run both forms against the same
expected values. `LogicValueHelpers.hpp` now holds the gate rules as
functions, so a behavioral form gives the same result as the gates for
unknown inputs. Parents still build these parts by class, so no existing
circuit or default view changed.
