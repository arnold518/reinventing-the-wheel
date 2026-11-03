# Status

Updated: 2026-11-03.

This is the living project-wide view. Rewrite it when the implementation or
work state changes. Keep dated decisions and validation results in
[topic records](records/README.md), and completed changes in the
[changelog](CHANGELOG.md). Working rules are in [AGENTS.md](../AGENTS.md).

## What exists

- The normal visualizer offers named `gate`, `logic`, `visual` and `compact`
  fidelity presets beside timing, preserving scenario defaults and isolated
  custom overrides. Native and WASM use the same policy. Full device views
  have validated compact compositions. Structural gameplay recordings share
  streamed topology files and retain every PRESENT in bounded state windows;
  full-tick capture adds compressed signal, memory and register deltas with
  seekable snapshots and PRESENT markers. Scoped native/browser checks pass;
  full game recapture is pending below. Replay keeps its recorded fidelity
  fixed. Supported device-system presets
  now complete structure-specific browser layouts, preserve saved geometry
  and report unsupported Gate storage capacity clearly. See the
  [implementation and running campaign](records/2026-10-24-fidelity-presets-and-visual-gameplays.md).

- The [RV32IM/device milestone checkpoint](records/2026-10-22-rv32im-milestone-wrap-and-integration.md)
  is merged into `alpha`: structural/compact M components, a scripted device
  platform, game fixtures and recorded display/keyboard views. Current scoped
  native, portable and strict package checks pass; phase-6 acceptance remains
  open below. The primary pending work and deployed modules are preserved.
  The [game RAM review](records/2026-10-26-game-memory-layout-review.md) records
  the linker budgets and current ELF footprints; peak stack use is unmeasured.
- CircuitSim has an event-driven C++20 engine, structural and behavioral
  component families, memory devices and RV32I cores. The public component
  and simulator reference is the [MkDocs source](../site-docs/index.md).
- The browser visualizer runs with a native backend or WebAssembly. The
  `github-pages` workflow now triggers from `alpha` and builds and tests the
  combined documentation and simulator site before deployment. Its trigger
  commit was pushed; the final live URL was not checked. See the closed
  [Pages readiness record](records/2026-10-16-github-pages-readiness.md).
  Its toolbars and panels use one dark theme with icon buttons since the
  closed [visual refresh record](records/2026-10-21-visualizer-visual-refresh.md);
  the published site still has the old look until the next Pages build.
- Development uses `alpha` directly in the main repository; no linked Git
  worktrees remain. Older pending changes are saved in a verified stash and
  backup, and former build/evidence directories are ordinary ignored archives.
  Port 8893 serves the main documentation directory and current milestone
  simulator package, with [shared package selection](records/2026-10-29-local-preview-recovery.md)
  for assets and example rows. The replacement ALU references now use `/alu/`; the
  original canvas now has current native recordings for all ten packaged
  game fixtures, with 47 exact-reference checkpoints. The refreshed record
  index, system page and simulator pass live route checks, and all 79 checked
  recording/evidence/simulator assets match their hashes. The grouped
  M-unit/execute views and dedicated program playback pass focused browser
  checks; live Fibonacci inspection also passes halt and rewind. Its strict
  preview build passes with the per-build
  timing-cache fix loaded; scoped acceptance is in the completed records below.
  Bring-up/2048 frame-only exports retain their source snapshots.
  The ten fixtures and their acceptance campaign were retired on 2026-11-03
  ([game scenarios](records/2026-11-03-game-scenarios-plan.md)).
  The preview service is capped at 768 MiB with no swap and builds in a managed
  disk cache after the [tmpfs/OOM recovery](records/2026-10-26-preview-memory-recovery.md).
  See the closed
  [main checkout consolidation](records/2026-10-22-main-alpha-checkout.md) and
  earlier [local preview record](records/2026-10-22-alpha-local-preview.md).
- Scalable memory endpoints use a visible controller with each technology's
  behavioral native device. The redundant memory container contract is
  retired; current profiles use native-device IDs and retired IDs are rejected. The [retirement record](records/2026-10-16-native-memory-device-retirement.md)
  reports 28 focused native and seven local web checks plus the Pages build.
- The [test and scenario guide](guides/tests-and-scenarios.md) documents the
  registry, focused checks, visualizer aliases and generated examples; its
  [record](records/2026-10-16-tests-and-scenarios-guide.md) is closed.
- The completed [component replacement](records/2026-10-22-general-component-retirement.md)
  implements shared word/mux/decoder/register-file/M-unit/bus/pixel recipes,
  removes fixed components and retired-ID aliases, and updates public ALU/RV32I
  references, scenarios and saved layouts. The current ISA exceptions are
  `RegisterFile32x32` and `RV32IALU`. Its checkpoint passed focused
  native/API/reference/portable/browser checks, all 158 then-current example
  groups and complete unit timing-file verification. The three profiles had 809 entries
  each. The [width record](records/2026-10-22-width-component-audit.md) is closed;
  full game acceptance remains below.
- The completed [ALU interface review](records/2026-10-22-alu-exposure-review.md)
  exposes width-only Chapter 2 components at 8/16/32 bits with prefix
  arithmetic and a separate fixed-width `RV32IALU` with six parts. References,
  contracts, bindings, scenarios and saved identities follow that interface.
  Serial native/WASM builds, focused native checks, all 156 current example
  groups, strict package/preview builds and nine selected web checks pass.
  The 18 affected table families verify in every timing profile; compiled
  native/WASM snapshots match at 809 entries per profile. Migrated 44 layout
  scopes: 367 scenarios cleared the complete scan, and the final fetch-root
  correction passes a focused recheck. Full suites and game acceptance remain
  outside that selection.
- The completed [device parameter/view work](records/2026-10-21-device-parameters-and-visuals.md)
  covers configurable geometry/queues, declarative maps and observational
  display/keyboard views. Current native/API checks cover direct 320x200 and
  CPU high-address presentation at 128x96/97x65. Broader game/performance and
  interactive acceptance remain in the game milestone.
- The completed [RV32IM scenario visualization fixes](records/2026-10-23-rv32im-scenario-visualization-fixes.md)
  give 22 component demos descriptive roots and structural architecture
  defaults, repair missing expansion layouts, preserve saved `DUT` profile
  paths and give MulDiv a readable engine floorplan. The native build, 28
  focused registrations, 25 static layouts, 22 native/WASM comparisons,
  strict site with all 156 example groups and 22-page/23-session browser
  selection pass. The final MulDiv/I-core recheck and live 8893 checks pass.
- The completed [RV32IM grouping and program scenarios](records/2026-10-23-rv32im-grouping-and-program-scenarios.md)
  replace flat arithmetic/extension views with structural groups and expand
  the ten M component recordings to 7–49 rows each. Three dedicated programs
  cover all M operations, memory/branch dependencies and divide/sign edges,
  with oracle-checked commits and retained timelines. The focused native,
  timing, migration/layout, 14 WASM example groups and four browser sessions
  pass, followed by a two-session floorplan recheck and current 8893 acceptance.
  The standalone MulDiv unit period is now 68 ticks; the core remains 94.
  The complete native and browser suites pass (2026-11-03); the game
  campaign was not run.
- The completed [RV32IM algorithms and inspection fixes](records/2026-10-23-rv32im-program-expansion-and-inspection.md)
  center EXECUTE in the pipeline, decode M instructions in memory views for
  structural/compact M machines and number programs 23–27. New fast-doubling
  Fibonacci and Euclidean GCD programs use real multiply/divide instructions,
  verify memory arrays and check retired M-operation counts. Eight focused
  native checks, six static layouts, alias/layout migrations, five WASM
  program exports and two browser sessions pass. The strict combined site
  and 8893 preview rebuilds pass, with eight routes and 26 exact asset hashes.
  No hardware/timing-table change, full suite or game campaign was needed.
- The completed [structural constant cleanup](records/2026-10-24-structural-constant-words.md)
  shares scalar ties and builds fixed buses from constant words across
  arithmetic/device wiring; SRAM/ROM response wiring also shares its zero
  source. Both 32-bit arithmetic datapaths have one zero-word source and
  14 children instead of 46. Step references show `<W>` and the menu shows
  their recorded `<4>` width. Focused native, unchanged timing, layout,
  native/WASM and browser checks pass, with the strict site and 8893 preview
  updated. Complete suites remain outside this selection.
- The completed [Device I/O hierarchy and layout review](records/2026-10-24-device-io-hierarchy-and-layouts.md)
  groups all twelve device scenarios into functional structural parts and
  gives the integrated system an explicit overview floorplan. The later
  [complete menu review](records/2026-10-24-scenario-menu-order.md) applies
  the overall component order while retaining these functional groups.
  Twenty-seven focused native checks, timing-table
  comparisons, sixteen static layouts, saved-setting migrations, twelve
  native/WASM comparisons and the device browser selection pass. Strict
  site/preview builds and final live system geometry checks pass on 8893;
  fourteen routes and 26 exact assets are verified. Full suites, expanded-core
  timing certification and the game campaign remain outside this acceptance.
- The completed [scenario menu review](records/2026-10-24-scenario-menu-order.md)
  orders all 26 groups and 219 visible rows from foundations/ALU/memories
  through RV32I/programs, RV32IM/programs and Device I/O, with developer
  circuits last. Explicit ranks replace browser alphabetization; names,
  aliases and saved identities are retained. Five focused catalog checks,
  strict packaging/preview builds and the complete live WASM menu check
  pass on 8893. The frozen long-game campaign continues independently.
- Games (§ 5.6, 2026-11-03): each game has a short version, a live scenario
  run with every check under **Computer System / Games**, and a long
  version, a computer player's session recorded at every frame. The ten
  earlier fixtures, their recordings and gameplay record set were retired;
  see [game scenarios](records/2026-11-03-game-scenarios-plan.md).
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
- The rewritten [§4.8 results report](../site-docs/rv32i/experiments.md)
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

- The closed [general cache plan](records/2026-10-19-general-cache-plan.md)
  made the cache's behavior configurable: write buffer, write-back and
  write-allocate, a line-state store (up to 4,096 sets), and 1/2/4/8 ways
  with lru, plru, fifo or random replacement. `controller=behavioral` runs
  the controller's own gate list as one compact part, cycle-identical and
  4–7× faster. A 96-run behavior campaign is published in the §4.8 memory-systems results:
  a two-entry write buffer gives 1.18–1.57× and removes most of the L2
  penalty. Prefetch is deferred (one request at a time).
- The closed [memory chapter rewrite](records/2026-10-18-memory-docs-rewrite.md)
  covers `site-docs/memory/` (3.1–3.8). Its site build, browser and parity
  checks pass after deterministic gate order in the memory controllers
  (native and WebAssembly had listed parts differently) and figure label
  fixes. The closed [shared layout keys record](records/2026-10-19-shared-layout-keys.md)
  gave each internal structure its own saved floorplan; cache controller
  placements were regenerated.

- The closed [memory systems record](records/2026-10-20-memory-systems-section.md)
  added §4.7 Memory Systems: the advanced core's five memory arrangements
  (separate, shared through the arbiter, L1s, L2, both), each with a
  visualizer scenario. The results page is now §4.8, with one memory-systems
  section (arrangements × cache behaviors). §3.8 keeps the cache-alone
  access-pattern experiment.

- The closed [Device I/O architecture record](records/2026-10-28-device-io-architecture.md)
  built chapter 5 (it replaces the RV32IM and Device I/O chapters). A machine
  is a JSON description (`systems/*.json`) built by `system.computer`: buses
  of per-master routers and two-port arbiters, pass-through or registered
  bridges, a memory subsystem (§ 4.7's shared arrangements and the § 4.8
  experiments build on it too), devices on generated register-table front
  ends, a port checker for every link and an LED panel added by description
  only; guest headers are generated. The one-bus machine equals the retired
  fixed system cycle for cycle; two buses cost one cycle per device access, a
  registered bridge three. Every scenario opens in a named fidelity profile
  (`scenarios.json`); § 5 layouts and colours were reviewed. The machine
  runs and clock certificates were redone on 2026-11-02 after the last
  structural changes; the games and the machine comparison moved to new
  fixtures on 2026-11-03 (below). Status choices use word
  gates (2026-11-02). Checks: one full native suite run (15 stale failures
  fixed and rerun), targeted tests after each later change, and the web
  checks covering the changes. On 2026-11-03 the full native suite (607/607)
  and the complete browser suite pass (48 checks; `TickRecordingBrowserTest`
  skipped, as it needs an external capture), after 11 stale checks, two
  layout lookup gaps, a no-op shipped profile and the games page default
  were fixed.
  The closed [chapter completeness review](records/2026-10-29-computer-system-documentation-review.md)
  adds precise interfaces, reference code and generated timing rows. It also
  records narrower checker/register-test coverage and the configurations
  needing additional timing or execution evidence.
  Its [catalog follow-up](records/2026-10-29-computer-system-compact-availability.md)
  corrects explicit compact step/decode/coordinator availability.

## In progress

- [Long games in Visual](records/2026-11-03-visual-long-games.md): prepared,
  not started. Recordings are now stored compressed (Compact: 58 → 3.5 MB);
  a Visual smoke campaign passed and opens in the viewer. The six-game
  Visual campaign (about 25 hours) is ready to launch.

- [Game scenarios](records/2026-11-03-game-scenarios-plan.md): implemented
  and checked natively, in WebAssembly and in the browser. All six long
  recordings passed and are published (3.5 MB compressed); the viewer check
  was run on Sokoban and Pong. The full native suite passes (2026-11-03,
  after three stale checks were fixed); the complete browser suite was not
  rerun. Each game has a short version, a live scenario under
  **Computer System / Games** that reaches its key event in one or two
  presses on a 32 × 24 screen, and a long version, a computer player's session
  recorded at every frame. The five machine scenarios now run the bring-up
  program, and the display, timer, button and LED scenarios were
  strengthened.

- [Word grouping review](records/2026-10-28-word-grouping-review.md) and
  [behavioral gap review](records/2026-10-28-behavioral-gap-review.md):
  implemented. Repeated one-bit parts in the RV32I ALU, Chapter 2 shift,
  compare and result units, memory read and payload muxes, caches,
  sequencers, decoders and the pipeline's comparators now use word gates
  (any width 2–32) and word muxes with the same gates; compact forms exist for
  the RV32I ALU's six parts, and a generic compiled gate-list form serves 33
  listed contracts when a profile asks for it exactly. Every delay table is
  unchanged except the new ones; the complete delay-table verification passes
  in all three profiles (2026-10-29). Layouts are migrated: the full static
  check finds 0 problems in 374 scenarios and the layout tests pass. The full
  native suite passes (2026-11-03).
- [Visualizer: explorer tree and fidelity](records/2026-10-28-visualizer-fidelity-pictures.md):
  the explorer tree is the planned fidelity (expanded = structural,
  collapsed = behavioral, Apply & Run rebuilds to match; badges show the
  built fidelity); the canvas follows the tree at once for built parts, and
  parts that exist only after Apply are previewed in gray (build-only C++
  preview). On the 8893 preview. The running visualizer never writes
  `layout.json` (port 8765 retired); browser layout saves store only edits;
  two hover-picking bugs are fixed. Device View returns as a separate canvas
  toggle, initially off: show only outermost visual abstractions regardless
  of fidelity, preserving the session, timeline, draft and layout. With it
  off, behavioral parts retain their visual abstractions. All component
  outlines are solid; the explorer S/B badges identify fidelity. Focused
  device/memory-cell checks, the updated explorer/recorded-circuit browser
  checks on 8893 and the feature's strict site build pass.
  The complete browser suite, `VisualizerWasmBrowserTest` included, passes
  (2026-11-03).

- [Non-device improvement plan](records/2026-10-27-non-device-improvement-plan.md):
  the owner-selected order is test sweeps, then the `Logic` adapter fix, then
  word-wide gates with a gate-symbol visual, with engine work alongside
  (run-time pin width included). Done: phase A (test sweeps), B1 (Logic
  adapters for non-device recipes), C (24 word gate families with a gate-symbol
  visual, used for the M units' inversions; tables 809 → 833, existing entries
  identical in all profiles) and exact engine steps: adapter pin caching and env
  lookups (2–21% less CPU on gate-level tests), inline queue keys, allocation-
  free no-op wire updates, cached sink routes and the incremental clocked twin
  (equal to the old twin on 72,000 steps; `DelayRuleTest` passes). One paired
  A/B/B/A block per test: gate-level tests 21–33% less CPU, the compact 32-bit
  M unit 59% less, the five-stage core test 4.4% less; a one-byte `LogicValue`
  adds about 2%; typed fanout copies up to 6.6% more. On the record's Visual
  guest (2,000 cycles) execution falls 36% against the frozen 2026-10-26
  baseline (1.04 M → 1.56 M events/s); the 10 M events/s target remains open.
  Twelve key tests also pass in the WebAssembly test runner, and a native
  browser check confirms word gates draw closed and open on selection; the
  browser WASM module and web parity tests are not rebuilt. B2 via `AddSub`
  was rejected for changing core timing; word gates in Chapter 2 components
  are now used too, after the owner made the bus splitter zero-delay in `unit`
  (it already was in the realistic profiles): `embedded`/`desktop` tables are
  byte-identical, `unit` was regenerated and re-certified in two table passes (cores 142/68/80,
  RV32IM 84). All 373 scenarios pass the static layout check.
  D2 (run-time pin width) is done: values of 1–64 bits are packed in two
  words, one wire event class serves every width, and the fixed width list is
  gone. The broad non-slow native suite passes 362/365; the three failures
  predate D2 (`DeviceParametersBindingsTest` from the uncommitted device work,
  two visualizer palette/profile tests), and `DelayRuleTest` passes. Test-level speed is unchanged within
  noise (load 3–4); the Visual guest is 2% faster with 12.5% more events.
  The `unit` tables needed a second pass (the first proved parts on stale
  storage tables): 117 entries fell, a third pass is identical, and the
  advanced core's certificate is now 80. The last unused splits (inside the
  multiply/divide steps) are removed. Pooled events are in: the Visual
  guest runs 6.3% faster with the same events (1.98 M events/s). The WASM
  rebuild passes 16 runner tests; all 161 example groups verify and the
  strict site build passes; native/WASM parity passes 16 of 16 (all ten
  recorded guests exact) after two stale test expectations were fixed; the
  three visualizer browser checks pass (one after updating word-gate
  component counts).
  Still open: the published §4.8 results at the old `unit` periods (owner
  decision). The full native suite passes (2026-11-03).

- [Device architecture review](records/2026-10-27-device-architecture-review.md)
  and [Device I/O and bit-adapter review](records/2026-10-27-device-io-review.md):
  their findings (fixed map, hand-wired system, no bridge, legacy adapters in
  device recipes) are addressed by the Device I/O architecture above; the
  device recipes now use the minimal adapters in their new parts. Interrupts,
  DMA and trap handling remain outside the scope. Both records stay open
  until the architecture record closes.

- [Test strength review](records/2026-10-27-test-strength-review.md): the
  audit of 464 registered tests found thin 16/32-bit word arithmetic
  (`ParallelPrefixAdderWidth16Test` has 8 cases), gate-level multiply/divide and
  small RV32I control blocks. Phases 1–4 are done: adder, ALU and RV32IALU
  sweeps, fully gate-level M units (`MulDivGateLevelTest`, slow), forwarding/
  hazard enumeration and ROM extras, each catching a deliberate fault that the
  old tests miss, plus a fix for a pre-existing catalog ownership conflict.
  Focused CTests pass; the full suite, WASM and strict site build were not
  run. Bus/router sweeps are deferred with Device I/O.

- [Engine performance investigation](records/2026-10-26-engine-performance.md):
  primitive gate input caches with immediate invalidation and headless value
  reuse pass a full isolated native rebuild, thirteen focused native checks,
  full-tick decoding and exact stored-delay comparison. Earlier private wire
  and selected-file compiler comparisons show roughly 3% lower CPU in limited
  stable brackets; wire allocation requests fall 32%.
  Stateless memo gets no hits, guarded clocked memo gets 0.3% and adds allocations;
  both are rejected for this workload. Timing scratch reuse has negligible
  allocation impact. Production remains at the cache-enabled baseline.
  Ordered-mask incremental clocked twins now skip 66.1% of modeled evaluations
  with about 79 KiB cumulative metadata. The latest phase passes 58 focused
  checks, 82 guests and 41 golden-matching traces. Stable `-O1` guest blocks
  show 4.9% / 4.2% lower CPU without/with capture, but there is only one stable
  block per mode; all `-O2` blocks are unstable. Large register-kernel gains
  are separate from whole-engine throughput. These remain private prototypes;
  broader fallback/workload/WASM validation and integration are pending.
  Other candidates are typed propagation, behavioral execution routes and
  bounded event reuse. Hybrid scheduling needs equivalence work; the old
  bucket prototype regresses sparse queues 2.2–3.4x. Lossless verification
  archiving preserves evidence with about 2.36 GiB net disk savings.
  Latest experiment peak RSS is 262 MiB with no experiment OOM/swap.
  Broader workload/WASM validation and 10 million events/second remain open.
  Live modules/recordings and the stopped gameplay queue are preserved.
  The 2026-10-27 review puts native cost at about 1 µs CPU per event,
  10–12× from the target; measured allocation savings explain only a few
  percent. It lists new exact-cleanup candidates and a six-phase plan with
  seven pending owner decisions. No new measurement was run.

- [FF controller startup](records/2026-10-21-memory-controller-startup.md):
  initialization preserves provisioned images in structural, fused and mixed
  profiles. Current native startup and all three large SRAM subarray/clocked/
  contract checks pass; the latter peak at 764,540 KiB RSS. Current WASM
  acceptance is in progress.

- [Compiler memory prerequisites](records/2026-10-21-delay-table-compile-memory.md):
  optional table partitions and split binding registrations build within the
  local caps. The preceding checkpoint preserved the 175-name legacy Python
  snapshot and five later additions, with three new exports. The ALU interface
  change intentionally replaces its configuration API. Current native/WASM
  timing snapshots match at 809 entries per profile, and all 156 current
  example groups regenerate in isolated processes; see the ALU record above.

- [64-bit simulation time prerequisite](records/2026-10-21-simulation-time-width.md):
  explicit ticks, checked scheduling, wide histories and exact JS transport
  pass focused native/Python/WASM checks, including device runs above 2^53
  and 2^63. The full current-source suite is not claimed.

- [RV32IM and device foundation implementation](records/2026-10-20-rv32im-phase0-3-implementation.md)
  implements phases 0–3 of the [adopted roadmap](records/2026-10-19-device-io-games-plan.md#rv32im-scripted-roadmap).
  The separate M pipeline and structural device contracts retain the existing
  RV32I families and recorded playback. Current scoped core/device/transport
  checks pass; full milestone acceptance is tracked above. Builds and evidence
  now use main `build-milestone` / `build-wasm-milestone`; older archives remain
  ordinary ignored directories. There is
  no longer a Git worktree. No live viewer module was overwritten.
  See the [foundation contract](reference/computer-system.md).

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
  tests. The documentation phase is done (the memory chapter record above).

- [Future steps plan](records/2026-10-14-future-steps-plan.md): current CPU,
  memory and viewer boundaries assessed against interactive games, a
  general-purpose GPU and AI accelerators. The owner chose cache work,
  then games, then AI accelerators; the GPU goal is
  unscheduled. Scalable FF timing is address-independent within one
  configuration; DRAM latency depends on row state and refresh.
  The owner selected RV32IM and predetermined device input first. The
  adopted roadmap starts scripted games with 2048, then Sokoban, Snake,
  Pong and Tetris, after the integrated machine and capture adapters. See
  the [device/game roadmap](records/2026-10-19-device-io-games-plan.md#rv32im-scripted-roadmap).

- [Cache system plan](records/2026-10-14-cache-system-plan.md): the
  standalone `Cache<W>` for W=8/16/32 and the five-topology cache campaign,
  published in [3.8](../site-docs/memory/cache-experiments.md). The L1 pair
  gives 3–4× over shared uncached DRAM; with write-through caches, adding the
  L2 under the L1s is 4–8 % slower in three of four workloads. The cache's
  current settings are those of the general cache plan above. There is no cache coherence; the new device platform
  routes MMIO outside caches. The two-port arbiter is documented in §3.5.3, and
  shared code and data images that overlap are refused.
- [Behavioral delays and the memory clock-edge rule](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md):
  code and checks are done locally and not pushed. Published results for
  the selected SRAM, DRAM, ROM and NOR-flash workloads have been rerun; other
  configurations remain outside that measured panel.

- [Documentation audit](records/2026-10-13-documentation-audit.md): public
  component behavior and inventory corrections are checked. Behavioral delay
  and clock-contract descriptions now reflect the current implementation;
  public landing/visualizer pages and minor non-component findings remain
  open. The RV32I results page is tracked in the separate record below.


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
