# Status

Updated: 2026-10-14.

This is the living project-wide view. Rewrite it when the implementation or
work state changes. Keep dated decisions and validation results in
[topic records](records/README.md), and completed changes in the
[changelog](CHANGELOG.md). Working rules are in [AGENTS.md](../AGENTS.md).

## What exists

- CircuitSim has an event-driven C++20 engine, structural and behavioral
  component families, memory devices and RV32I cores. The public component
  and simulator reference is the [MkDocs source](../site-docs/index.md).
- The browser visualizer runs with a native backend or WebAssembly. The
  `github-pages` workflow builds and tests the combined documentation and
  simulator site before deployment.
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
  order is standalone structural cache with one future scalable-cache
  contract, measured/scoped scalable-memory timing plus one shared RAM image,
  then equivalent scalable cache components and program studies. The advanced
  core keeps two request paths; a 2:1 arbiter at one backing port is the
  proposed first integration. Geometry, write policy, timing extrapolation,
  code-store visibility and later DMA sharing remain design choices. A
  source review found that common REQ/RSP endpoints can serve cache tag/data
  banks and backing memory without a technology-specific pin adapter. Their
  supported operations and timing still differ. A detailed standalone
  structural-cache proposal now defines a single external cache contract,
  independently selectable endpoint banks, build increments and proof gates
  grounded in the local textbook. No cache or unified-RAM platform exists yet.

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
  The strict MkDocs build passed with existing generated examples; the
  WebAssembly package and browser checks have not been rerun since `34abf09`.
