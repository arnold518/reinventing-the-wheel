# Status

Updated: 2026-10-16.

This is the living project-wide view. Rewrite it when the implementation or
work state changes. Keep dated decisions and validation results in
[topic records](records/README.md), and completed changes in the
[changelog](CHANGELOG.md). Working rules are in [AGENTS.md](../AGENTS.md).

## What exists

- CircuitSim has an event-driven C++20 engine, structural and behavioral
  component families, memory devices and RV32I cores. The public component
  and simulator reference is the [MkDocs source](../site-docs/index.md).
- The browser visualizer runs with a native backend or WebAssembly. The
  `github-pages` workflow now triggers from `alpha` and builds and tests the
  combined documentation and simulator site before deployment. Its trigger
  commit was pushed; the final live URL was not checked. See the closed
  [Pages readiness record](records/2026-10-16-github-pages-readiness.md).
- Scalable memory endpoints use a visible controller with each technology's
  behavioral native device. The redundant memory container contract is
  retired with saved-ID aliases. The [retirement record](records/2026-10-16-native-memory-device-retirement.md)
  reports 28 focused native and seven local web checks plus the Pages build.
- The [test and scenario guide](guides/tests-and-scenarios.md) documents the
  registry, focused checks, visualizer aliases and generated examples; its
  [record](records/2026-10-16-tests-and-scenarios-guide.md) is closed.
- Current internal guidance is indexed in [docs/README.md](README.md).
  Historical plans, retired guides and completed reports are listed in the
  [archive index](archive/README.md).
- The completed [RV32I test and experiment evidence review](records/2026-10-13-rv32i-test-experiment-review.md)
  distinguishes current test coverage from dated campaign results.
- The completed [RV32I experiment results audit](records/2026-10-14-rv32i-experiment-results-audit.md)
  inventories all complete local campaign directories and the rerun boundary
  for current performance claims.
- The [structural-run feasibility review](records/2026-10-14-rv32i-structural-run-feasibility.md)
  estimates the 35 long branch checks as practical and full compiled-program
  structural sweeps as multi-day or longer on this host.
- The [current-source long branch rerun](records/2026-10-14-rv32i-branch64-structural-rerun.md)
  completed 35/35 oracle, expected-result and structural checks at certified
  FF periods of 166/80/100 ticks.
- The rewritten [§4.7 results report](../site-docs/rv32i/experiments.md)
  gives 497 completed current-source panel case/target rows: 420 broad FF
  cases, 35 long branches, 18 compatible FF/timed-memory pairs, seven focused
  program-20 memory pairs, three predictor-capacity cases and 14 checked-in
  ELF cases. Its [completed documentation record](records/2026-10-13-rv32i-test-experiment-documentation-update.md)
  covers the performance analysis. The [structure rewrite](records/2026-10-14-rv32i-results-structure-rewrite.md)
  organizes results by the §4.2 workload groups, with matching tables and
  interpretations together. The [earlier presentation record](records/2026-10-14-rv32i-results-presentation.md)
  documents four figures withdrawn by the [chart reset](records/2026-10-14-rv32i-chart-reset.md).
  The [chart implementation](records/2026-10-14-rv32i-chart-implementation.md)
  adds four replacement figures with all seven comparable FF targets visible
  in each core comparison. The former public clock-development narrative is
  in the internal archive. Full Embench uses older cycles, not a current run.
- The [three-core performance comparison](records/2026-10-14-rv32i-three-core-performance-comparison.md)
  interprets current branch and program-20 results alongside historical
  core-gap and compiled evidence, with their clock and source limits.
- The [Embench core comparison](records/2026-10-14-rv32i-embench-core-comparison.md)
  quantifies the completed 133-case historical full suite and nine reduced
  structural cases; the full suite has not been rerun at final clocks.

## In progress

- [Realistic leaf delays and gate-level memory control](records/2026-10-16-realistic-timing-plan.md),
  executed on branch `realistic-timing` with a [progress log](records/2026-10-16-realistic-timing-progress.md):
  phases 1–3 are done. Leaf delays come from a timing profile (`unit`,
  today's timing and still the default; `embedded` 65 nm and `desktop` 32 nm,
  1 tick = 1 ps); gates use inertial delay; per-profile delay tables are
  generated to a fixed point. Under the realistic profiles the cores certify
  at 140–428 MHz with identical cycles and oracle-checked results. Phase 4
  (gate-level memory control, merged with device self-timing) has steps
  4.1–4.3 done under `unit`: SRAM/ROM and DRAM/NOR controllers are gates,
  the devices time themselves, compact forms are cycle-exact copies, and
  focused memory tests, the delay rule and oracle-checked CPU runs pass.
  Cycle counts with timed memories grew (synchronizer, clocked command
  protocol), so published timed-memory results hold only at tag
  `timing-legacy`. Open: step 4.4 (memory tables for the realistic profiles,
  CPU runs under all profiles), calibration, the cache experiment, then the
  public site and visualizer. `MemoryEndpointIntegrationTest` fails two
  example checks built from `site-docs/` snippets until that site update.

- [Future steps plan](records/2026-10-14-future-steps-plan.md): current CPU,
  memory and viewer boundaries assessed against interactive games, a
  general-purpose GPU and AI accelerators. The owner chose cache work,
  then games, then AI accelerators; the GPU goal is
  unscheduled. Scalable FF timing is address-independent within one
  configuration; DRAM latency depends on row state and refresh.
  RV32IM versus Zmmul is now an open pre-game ISA choice, with a pinned
  compiler check favoring RV32IM for C code generation. Open-source C
  references for 2048, Snake, Pong and Tetris have been shortlisted; the
  first game is not selected.

- [Cache system plan](records/2026-10-14-cache-system-plan.md): the adopted
  standalone `Cache<W>` exists for W=8/16/32. It has structural lookup and
  refill, up to two ways with LRU, invalidation and write-through writes.
  Expanded and scalable forms share pins and control while selecting FF/SRAM
  tag and data endpoints independently. Four public examples now cover a
  direct-mapped cache, a small two-way cache with writes, a larger scalable
  cache and a connected L1/L2 path over FF RAM. Native scenario, catalog,
  layout and documentation checks pass. The focused Pages/WASM build produced
  30, 116 and 58 verified example rows for the three single-cache scenarios;
  the two-level timeline has ten checkpoints. The focused browser test passed
  all four examples. The page keeps a compiled binary C++ reference for
  the cache algorithm. An optional advanced-core test path connects I/D
  caches through a structural arbiter to one shared RAM; short programs pass.
  The supported geometry is at most four expanded or 64 scalable sets, two
  ways and four words per line. No fused behavioral cache, coherence or MMIO
  bypass exists. The current short ROM/SRAM CPU fixtures show more cycles
  with caches (28 to 63 and 70 to 161), so no program speedup is established.
  Small-memory timing calibration and representative workload measurements
  remain open. The timing-model review distinguishes small structural/compact
  equivalence from physical calibration. Existing delay-rule comparisons
  support starting matched direct-versus-cached endpoint experiments now;
  per-request parity and CPU speed claims still require those measurements.
  The open record now specifies one focused performance stack: SRAM tag/data
  banks in separate I/D L1s and a shared L2, over DRAM main memory. It plans
  a small structural/scalable timing check, three standalone paths and five
  CPU topologies: two separate memories, shared uncached RAM, both L1s,
  shared L2 alone, and both L1s over L2. Other technologies and I-only/D-only
  runs are targeted diagnostics, not full performance panels. The bypass/L2
  CPU wiring, benchmark fixtures and runs are not yet built. SRAM cache-hit
  timing and the cached CPU clock still need measurement before performance
  claims. A 2026-10-16 latency probe (scratch program, not a CTest) measured
  an SRAM-bank L1 hit at 14 cycles against 4 for a direct DRAM open-row read
  and 10 for a row conflict at period 102. The planned campaign would almost
  certainly show every cached path slower. The campaign is deferred until
  the realistic-timing plan is finished; the record lists what must be
  restated then (timing profile, DRAM geometry, cache sizes, bursts).
  The public [cache reference](../site-docs/memory/3.4.md)
  states the current contract and examples.

- [Behavioral delays and the memory clock-edge rule](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md):
  code and checks are done locally and not pushed. Published results for
  the selected SRAM, DRAM, ROM and NOR-flash workloads have been rerun; other
  configurations remain outside that measured panel.

- [Documentation audit](records/2026-10-13-documentation-audit.md): public
  component behavior and inventory corrections are checked. Behavioral delay
  and clock-contract descriptions now reflect the current implementation;
  public landing/visualizer pages and minor non-component findings remain
  open. The RV32I results page is tracked in the separate record below.

- [Engine performance](records/2026-10-13-engine-performance.md): measured
  and profiled; compact cores run at 3,700 to 13,400 cycles per second and
  the gate-level advanced core at 51. An eight-phase plan is proposed and
  nothing is implemented. Three owner decisions are open.

## Open items

- The 18-case timed-memory panel gives current results only for its selected
  compatible workloads and advanced/two-bit setting; other memory/core/policy
  combinations need separate checks before claiming their performance.
- The delay record flags advanced/FF measurements at period 98, while the
  current certified period is 100. This audit found no completed
  `build/experiments/` campaign at 98; identify the original measurements
  before revising that published claim. Verify unchanged cycles before
  scaling modeled time.
- The current broad FF panel completed 420/420 cases on `448e4e3`, while the
  full 19-workload Embench result is projected from historical cycles and
  still needs a current-source rerun to become a measurement.
- A prior full native suite passed after a clean rebuild at `448e4e3` (451 of
  451). The latest non-Embench native invocation accounted for 450 tests
  across a 449/450 sandbox run and a separate 1/1 retry outside the sandbox.
  The current focused Pages/WASM build and memory endpoint browser check pass
  on the uncommitted cache work. The full native and browser suites have not
  been rerun for this worktree.
