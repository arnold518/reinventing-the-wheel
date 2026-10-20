# Status

Updated: 2026-10-20.

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
  phases 1–6 are done. Leaf delays come from a timing profile (`unit`, the
  default for tests; `embedded` 65 nm and `desktop` 32 nm, 1 tick = 1 ps)
  with inertial gate delay and per-profile delay tables at a fixed point. All
  memory controllers are gates, the devices time themselves, and compact
  forms take their timing from the gates; the delay rule passes in the CTest
  configuration. External DRAM (SDR SDRAM for `embedded`, DDR4-3200 for
  `desktop`) and NOR flash use datasheet intervals; the experiments' on-chip
  SRAM (`--memory sram`) is a clocked macro (the future cache bank), with the
  asynchronous chip kept as `sram-async`. Endpoints take requests from
  registered state only, so every memory, the FF reference included,
  certifies at the core's own clock (4,104 ps `embedded`, 2,858 ps
  `desktop`). A core sweep (all cores, predictors and programs, every memory
  the cores accept) verified 306 runs. Residuals, reported and not tuned: the
  clocked macro is 3.1× CACTI at 512 B; DRAM end-to-end latency is +23–33 %
  over a counted controller (one synchronizer edge per command). Cycle counts
  differ from the published ones, which hold at tag `timing-legacy`. The owner
  directed realistic memory models that are not tuned to favor caches.
  The STM32F7 system check is a plausibility estimate from secondary
  sources (RM0385 itself could not be read here): the DRAM row hit fits,
  closed and conflicting rows are 1–5 cycles high, as already recorded.
  Tick-driven tests (CacheReadTest, MemoryDeviceTimingTest,
  MemoryEndpointTimingTest) are `unit`-only. Phase 7: the cache looks
  up every way at once in directly driven banks (L1 hit two cycles, 7a), and
  burst reads (op 4) exist end to end (7b): SRAM, ROM and DRAM endpoints
  return a line from the requested word, and the cache fills lines with one
  burst (`fill=burst`). The cache experiment has run (exploratory, not
  published): Step 1 holds in both profiles (L1 hit 2, L2 hit 8, DRAM 8–22
  cycles); standalone S0–S2 and locality traces (sequential access 2.5×
  faster than row-hopping without caches, about 5× with an L1; caches slow
  down access with no spatial locality); the five-topology CPU panel and
  pointer-chase, all 40 runs verified: the L1 pair is 3–4× faster than the
  shared uncached bus, the L2 alone nearly as fast, and the L2 under the L1s
  2–8 % slower (write-through through two levels). The safe-clock analyzer
  now certifies every topology (A, B, C-ID, D-0, D-ID) at the core's clock
  (4104 / 2858 ps), the clock the campaign used: the caches hand their clock
  on through zero-delay wires, which the analyzer accepts, and the arbiter
  no longer gates its clock. Below that clock the setup checks report the
  cache's late inputs (389 at `embedded` 3300 ps for C-ID). Phase 8 under way: the gate-level clocked
  SRAM macro's write disturb (a finished write's drivers and word line still
  on when the next access's address arrived) is fixed by sizing its settle
  line; compact and gate-level banks agree again. The visualizer chooses the
  timing profile (native and WebAssembly). Final full native suite under
  `unit` at `dff56d5`: 467 of 470 (the three `site-docs` checks, since
  rewritten); all 13 browser smoke checks pass (run one at a time in a
  memory-capped scope). The suite is a `unit` suite by design; realistic
  profiles are covered by `RV32IProfileSweepTest` and other profile-selecting
  tests. The documentation phase is under way (below).

- [Memory chapter rewrite](records/2026-10-18-memory-docs-rewrite.md):
  `site-docs/memory/` is rewritten (3.1 architecture, 3.2 shared building
  blocks with the timing and control parts, 3.3 technologies with
  3.3.1–3.3.6 as subsections, 3.4 protocol, 3.5 controllers and endpoints,
  3.6 memory timing, 3.7 caches kept for their own rewrite) and a shared
  "§ 1. Time and Delay" page added. Components whose timing has something
  of its own to show (17: adders, ALUs, flip-flop, register, register file,
  memory devices, controller, endpoint, single-cycle core) end their
  implementation with a **Timing** paragraph and a generated table of the
  paths that matter; the stock Timing sections of the other 67 were
  removed; 4.2 has per-profile certificates; 4.7 states results in cycles
  with per-profile summaries and publishes the cache campaign; eight new
  figures and the four 4.7 plots follow the site palette. All 18 native
  documentation checks pass. The cache pages (3.7, 3.8) are rewritten with
  the general cache plan below. **Open:** a memory-gated run to rebuild the
  site and run the browser and parity checks still owed on the current pages
  (results go in the record).

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

- [Cache system plan](records/2026-10-14-cache-system-plan.md): the
  standalone `Cache<W>` for W=8/16/32 and the five-topology cache campaign,
  published in [3.8](../site-docs/memory/cache-experiments.md). The L1 pair
  gives 3–4× over shared uncached DRAM; with write-through caches, adding the
  L2 under the L1s is 4–8 % slower in three of four workloads. The cache's
  current settings are those of the general cache plan below. No coherence or
  MMIO bypass exists. The two-port arbiter is documented in §3.5.3, and
  shared code and data images that overlap are refused.

- [Shared visualizer layout keys](records/2026-10-19-shared-layout-keys.md):
  saved floorplans were shared by parts of different internal structure
  (8- and 16-row SRAMs, FF stores of different depth, predictor policy
  variants, memory timers), and SRAM floorplans were placed before their
  children's proportions were known (the scalable cache's stores were drawn
  in a 2 % strip). Fixed with structure keys in the Python server and the C++
  session, a seeding fix, regenerated placements and a guard in
  `VisualizerStaticLayoutTest`; 13 native layout checks pass. **Open:** the
  WebAssembly rebuild and browser checks.

- [General cache plan](records/2026-10-19-general-cache-plan.md): implemented
  on branch `realistic-timing` (not pushed).
  - The cache gained a write buffer, write-back and write-allocate, a
    line-state store (up to 4,096 sets), and 1/2/4/8 ways with lru, plru, fifo
    or random replacement.
  - `controller=behavioral` runs the controller's own gate list as one
    compact part: cycle-identical in a lockstep test, the standalone traces
    and CPU runs, and 4–7× faster.
  - A 96-run behavior campaign (all verified, certified at 4,104 / 2,858 ps)
    is published as §3.8 experiment 3: a two-entry write buffer gives
    1.18–1.57× and removes most of the L2 penalty.
  - Prefetch is deferred (one request at a time).
  - **Open:** visualizer and browser checks (saved cache-controller layouts
    may need regenerating), waiting for a memory-gated rebuild.

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
