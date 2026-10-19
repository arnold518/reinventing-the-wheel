# Changelog

Record meaningful completed changes here, newest first. Each entry states
what changed and why, what was verified, and any limits or deliberate
omissions. Entries begin with adoption of this recording workflow; older
history remains in Git and the archived reports.

## 2026-10-19

### Component timing: only where it says something, inside the implementation (branch `realistic-timing`)

- **Changed:** 67 stock per-component Timing sections removed (a fixed
  sentence plus, at most, the full path table). The 17 components whose
  timing has something of its own to show (adders and ALUs, flip-flop,
  register, register file, memory devices, controller, endpoint,
  single-cycle core) end their implementation with a **Timing** paragraph:
  what sets the delay, and a generated table of only the paths that matter
  in every profile, through the new `<!-- timing-rows -->` marker. The
  paragraph ends "Behavioral implementation" (structural for `FullAdder`
  and `DFlipFlop`, which have no compact form), so component pages have five
  headings again. The paragraphs state facts inline instead of linking to
  § 1, § 3.6 or the 4.2 certificates; DRAM and NOR show their latency rows
  and the core its certified clocks directly. The template descriptions on
  § 1, the ALU8 conventions, 4.3 and 3.1 follow.
- **Why:** the owner found the sections disappointing and asked to remove
  those with nothing to say, not to send readers to the timing chapters,
  and suggested merging timing into the behavioral implementation.
- **Verified:** all 18 CTest `documentation` checks, including the updated
  heading checks and `TimingTableDocumentationTest` with the new marker and
  a guard against `### Timing` headings and timing-chapter links. The Pages
  rebuild and the browser checks run in a memory-gated run; see
  [the record](records/2026-10-18-memory-docs-rewrite.md).

### Timing across the site: clock certificates, results per profile, figures and plots (branch `realistic-timing`, documentation phase)

- **Changed:** 4.2 gains per-profile certified clocks (generated) and the
  cached-root and clock-wire rules; 4.7 states its results in cycles with
  per-profile summaries from the current certificates (advanced two-bit
  now leads 13/14 nontrap fixed programs); the timed-memory rows of the old
  memory design are replaced by a pointer to 3.6; the visualizer page
  describes the Timing selector; the home page gets an overview. Seven new
  figures in the site's figure format and colors (gate delays per profile,
  the SRAM access chain, DRAM reads by row state, read latency by
  technology, the transaction handshake, the SRAM device, the ALU8
  datapath), charts drawn from the generated timing data; the four § 4.7
  plots regenerated at the `embedded` clocks in the site palette. A
  diagram-design profile (`circuitsim`) holds the site tokens; the project
  marker `.diagram-design` selects it. Browser and parity tests follow the
  new memory URLs.
- **Why:** the owner asked for timing-aware pages, a site-wide figure
  review with diagram-design in the site's design, and no hand-typed
  timing numbers.
- **Verified:** strict MkDocs build; `DocumentationDiagramsTest`,
  `RV32IBenchmarkDocumentationTest`, `MemoryDeviceDocumentationTest`,
  `TimingTableDocumentationTest`, `DocumentationTimingDataTest`; the plot
  script validated all 602 cycle cells against the campaign data; figures
  inspected in light and dark; `tools/build-pages.sh` (before the caches
  section). Browser checks: 4 of 19 passed before the run was stopped for low
  host memory; the other 15 are not run.
- **Also:** § 4.7 publishes the cache campaign (five memory topologies,
  four workloads, both realistic profiles, standalone locality traces),
  tables generated from the run records, with a topology figure.

### Memory chapter rewritten; timing sections on every component page (branch `realistic-timing`, documentation phase)

- **Changed:** `site-docs/memory/` restructured and rewritten against the
  current code: 3.1 Memory Architecture, 3.2 Shared Building Blocks (with the
  timing and control parts), 3.3 Memory Technologies (3.3.1–3.3.6 as
  subsections), 3.4 Transaction Protocol (READ_BURST, registered
  `REQ_READY`, `LAST`/`SPACE`), 3.5 Controllers and Endpoints (gate-level
  sequencers, current configuration), 3.6 Memory Timing (new), 3.7 Caches.
  New top-level "§ 1. Time and Delay". The memory, ALU8 and RV32I 4.3–4.4
  component pages split "Unknown inputs and timing" into "Unknown inputs"
  and a "Timing" section with generated delay, interval and clock tables;
  the abstract-tick disclaimers are gone. Long controller and endpoint C++
  listings are replaced by a request check and an endpoint response
  function. Links from the RV32I pages follow the new addresses.
- **Why:** the owner asked for a complete memory rewrite after testing, with
  the technologies under one section, and for the site to state timing from
  the code (profiles, proven tables, measured latency).
- **Verified:** strict MkDocs build; `MemoryDeviceDocumentationTest`,
  `MemoryEndpointIntegrationTest` (the documented request check equals the
  production check in every case), `DocumentationExamplesTest`,
  `RV32IBuildingBlocksDocumentationTest`, `RV32ISingleCycleDocumentationTest`,
  `RegisterFileDocumentationTest`, `RegisterDocumentationTest`,
  `CacheDocumentationTest`, `DocumentationDiagramsTest`,
  `TimingTableDocumentationTest`.
- **Not yet:** browser tests that open the old memory URLs, the diagram
  review, 4.2/4.7/visualizer pages, the Pages build.

### Documentation timing data (branch `realistic-timing`, phase 8)

- **Changed:** new tool `documentation_timing_data` writes
  `tools/timing/documentation-timing.generated.json` (leaf delays, measured
  flip-flop and SRAM-cell contracts, certified periods, memory intervals,
  access latency of each technology at the advanced core's clock); new
  markers `profile-table` and `latency-table` in `tools/mkdocs_timing.py`.
- **Why:** no timing number on the site is typed by hand.
- **Verified:** `DocumentationTimingDataTest` (measured again, equal) and
  `MemoryLatencyGatesTest` (compact and gate-level endpoints take the same
  cycles in all three profiles, 38 of 38). Latencies equal the phase-6
  records.

### Static visualizer: complete layouts, compact Timing control, layout check (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` gains the 13 child placements the static site
  lacked (FF controller RUNNING/READY/NOTHING_HELD, a memory timer stage,
  the two-level cache's L1 variant); the compact view places the Timing
  selector beside the scenario; new `tools/check-static-layouts.py`
  (CTest `VisualizerStaticLayoutTest`) replays the browser's layout lookup
  for every scenario; new browser check `VisualizerTimingProfileBrowserTest`.
- **Why:** the endpoint scenarios stopped in the WebAssembly site on a
  missing placement, and the compact view hid the scenario selector.
- **Verified:** the Pages build (WebAssembly, example rows, strict MkDocs);
  browser checks `VisualizerWasmBrowserTest`,
  `VisualizerTimingProfileBrowserTest`, `MemoryEndpointBrowserTest`; the
  layout check over 339 scenarios (0 missing).

### Visualizer: choose the timing profile (branch `realistic-timing`, phase 8)

- **Changed:** a Timing selector (`unit`, `embedded`, `desktop`) beside the
  scenario, kept in the URL as `?timing=`; the circuit is built and
  simulated under it and times are shown in ps, ns, µs or ms when the
  profile's tick has a length. Native server: sessions per scenario and
  profile, the library's profile set under one lock while a session is
  built and restored afterwards; `timing` accepted by the circuit, state
  and apply requests; the topology response names the profile. Python
  binding `timing_profile_info`; WebAssembly `setTimingProfile` and
  `timingProfileJson`.
- **Why:** the realistic profiles were reachable only from the command line.
- **Verified:** `VisualizerTimingProfileTest` (new),
  `VisualizerReferenceProfileTest` (extended), the visualizer CTest group
  (only the known `site-docs/` check fails).
- **Limits:** the WebAssembly build and browser checks were not yet run
  when this was committed.

## 2026-10-18

### Program fixtures and fixed-time scenarios under every timing profile (branch `realistic-timing`, phase 8)

- **Changed:** `analysis::CERTIFIED_CORE_PERIODS` stores the certified FF
  clock of each core per profile (`rv32i_experiment
  --list-certified-periods`). The RV32I program fixtures keep their `unit`
  times and multiply them, under another profile, by the smallest whole
  factor that reaches the certified clock (`rv32iFixtureTimeScale`; the
  baseline fixture on its half cycle, since it releases reset half a cycle
  before its first edge). `FullCircuitTest` and `CacheTwoLevelScenarioTest`
  scale their times by gate delays.
- **Why:** the visualizer is to offer the realistic profiles; every program
  scenario failed under `embedded` because its times were `unit` ticks.
- **Verified:** the 179 program and fixed-time scenarios under `embedded`
  and `unit` (all pass; under `embedded` 1 passed before); baseline
  scenarios under `desktop`; `RV32IProfileSweepTest` with the new check
  that the stored periods equal the analysis.
- **Limits:** the full native suite was not rerun for this change yet (it
  runs once more at the end of phase 8).

### Clock certified for cached and shared roots (branch `realistic-timing`, phase 8)

- **Changed:** the two-port arbiter is clocked by I_CLK instead of an AND
  of both port clocks (D_CLK must carry the same clock; pins unchanged).
  The safe-clock analyzer treats a zero-delay one-bit Rewire fed by the
  clock as part of the clock net and states it as an assumption.
- **Why:** the analyzer refused every cached or arbitrated root. The
  arbiter's gate put one gate delay of skew on its clock; the caches hand
  their clock to the banks through zero-delay wires, which the delay-table
  analyzer already accepted.
- **Verified:** `--safe-clock` on A, B, C-ID, D-0 and D-ID in `embedded`
  and `desktop`: all safe at the core's clock (4104 / 2858 ps), no
  diagnostics; `--period auto` runs C-ID and D-ID verified with 0 setup
  violations; B and D-ID `sort` reruns match the campaign exactly. Tests:
  SafeClockTimingTest (new clock-wire case), RV32ITopologyTest (new
  certified-clock case), MemoryEndpointSafeClockTest, RV32IExperimentTest,
  RV32IExperimentCLITest, RV32IProfileSweepTest, TwoPortMemoryArbiterTest,
  CacheReadTest.
- **Limits:** the period is the core's (its port setups dominate); the
  caches' own shortest safe clock is not computed. Endpoints behind caches
  are analysed with all arcs, without handshake-state cuts.

### Visualizer data for the cache and memory timing parts (branch `realistic-timing`, phase 8)

- **Changed:** `layout.json` drops entries of removed parts and stale cache
  variants, rebuilds the three cache scenario floorplans for the direct
  banks, and adds default layouts for new parts (memory timers, sequencers,
  request checks, cache bank controller). `palette.json` colors the cache
  family and the memory timing parts.
- **Why:** three visualizer tests failed on layouts saved before phase 7a
  and on new types without colors.
- **Verified:** CTest label `visualizer`, 28 / 29 (the failure is the
  unchanged `site-docs/` checks of MemoryEndpointIntegrationTest).
- **Limits:** a full automatic refresh would also move saved children of
  nine entries this branch did not touch; they were left unchanged. Browser
  and WebAssembly checks were not run.

### Delay twin: a flip-flop stuck at X is timed from every edge (branch `realistic-timing`, phase 8)

- **Changed:** `DelayTwin.cpp` renews a flip-flop's launch time at every
  clock edge (and unknown reset) while its value is X, not only when the
  held value changes.
- **Why:** the strict-settling delay rule failed on five memory
  controllers. Traced on `memory.rom.controller.32`: an earlier situation
  left the twin's state X while the gates resolved; X looked unchanging, so
  paths from DONE and LAST counted as quiet and REQ_READY settled one tick
  earlier in the compact part than in the gates.
- **Verified:** DelayRuleTest `--strict-settling` on the five controllers in
  `unit`, `embedded` and `desktop` (0 breaks, 0 target misses); full suite
  under `unit`, 461 / 467 (the six failures are the known documentation
  and visualizer-data checks).

### Test timing audit: realistic profiles covered by their own tests (branch `realistic-timing`, phase 8)

- **Changed:** new `RV32IProfileSweepTest` runs every core on programs 1,
  9, 16, 19 and 20 under `embedded` and `desktop` at the analyzer's
  certified clock (single-cycle and baseline with FF memory; advanced with
  FF, on-chip SRAM and DRAM), oracle-verified, no setup violation. The tests
  guide states that the native suite is a `unit` suite.
- **Why:** under `embedded` 217 of 466 tests fail because they are written
  in `unit` ticks (exact tick counts, or clocks and sampling points in
  ticks); rewriting them would duplicate the phase 6.5 core sweep. A
  harness-only scaling attempt fixed none and was withdrawn.
- **Verified:** full suite under `unit` (459 / 466; the failures are six
  documentation or visualizer-data checks and the strict-settling delay
  rule, recorded as an open delay-twin defect); full suite under `embedded`
  (249 / 466, recorded as the evidence); `RV32IProfileSweepTest` (50 runs,
  113 s).

### DRAM command counters in the experiments (branch `realistic-timing`, phase 8)

- **Changed:** the compact DRAM control counts the commands it accepts;
  `BehavioralMemoryEndpoint::commandCounts()` reports them and
  `rv32i_experiment` adds `DEVICE_<memory>_<command>` per measured window.
  Benchmark-record units for observations are now requests, words,
  commands or cycles (all were cycles).
- **Why:** the cache campaign's shared-memory slowdown was explained by DRAM
  row changes without measuring them. Measured: B's shared DRAM opens rows
  23× as often as A's two memories in `sort`.
- **Verified:** the A and B `sort` runs (cycle counts unchanged),
  `RV32IExperimentTest`, `RV32IExperimentCLITest`, `RV32ITopologyTest`,
  `MemoryEndpointDRAMTest`, `NativeMemoryDeviceContractTest`. The standalone
  cache benchmark with gate-level banks matched the compact banks exactly
  after the SRAM macro fix.

## 2026-10-17

### Clocked SRAM macro: no write disturb on the next access (branch `realistic-timing`, phase 8)

- **Changed:** the gate-level clocked SRAM macro's settle line (how long its
  registered address and data are held back when an access starts) now
  covers the row decoder's enable path and the write drivers' turn-off
  (`circuit::sram::clockedSettle`); the compact macro's withdrawal and the
  fallback access times use the same value. Tables regenerated.
- **Why:** a finished write left its drivers and word line on; at the next
  access the new address and data reached the array 50 ps before the old
  word line closed (`embedded`), writing the new word into the old row
  (writing word 0 changed word 4, and so on). Found as stale tags in a cache
  bank; compact banks were not affected. A first fix that ended the write
  drive at READY was withdrawn: it added a 0.7-ns false path to the proven
  read-data launch.
- **Verified:** a write-after-write disturbance test (0 pairs in all three
  profiles; 4 at `embedded` before), the SRAM device tests, tables at a fixed
  point and recomputed identically, DelayRuleTest on the SRAM parts in all
  three profiles, the on-chip-SRAM CPU panel (unchanged), the CLI test, the
  standalone cache benchmark (identical to the campaign run).
- **Limits:** every clocked SRAM access starts about 310 ps later at
  `embedded` (a real decoder-reset delay); certified CPU clocks and cycle
  counts did not change. Cached roots now fail below 3300 / 2300 ps
  (`embedded` / `desktop`), leaving 24 % margin at the campaign clocks.

### Cache experiment runs and locality traces (branch `realistic-timing`, phase 7d)

- **Changed:** `CachePerformanceBenchmark` gained `stride-line` and
  `stride-row` traces (the same 2048 reads with one word per line, or per
  DRAM row) and builds its parts with the campaign profile (compact banks,
  as the CPU runs); diagnostic options run one trace/path or chosen
  addresses. No model or timing change.
- **Why:** the cache plan's Step 2 and Step 3, and the owner's question
  whether locality-friendly code is faster.
- **Verified (results, not tests):** standalone traces in `embedded` and
  `desktop`, every read and the final DRAM image checked; the five-topology
  CPU panel (calls, sort, stream) and pointer-chase in both profiles, 40
  runs, all verified against the oracle. Key figures and the counter-based
  explanations are in the realistic-timing progress log.
- **Limits:** exploratory: cached roots are not certified (run at the core's
  clock with a measured margin); the campaign used compact banks while the
  gate-level macro has an open defect inside the cache; DRAM row changes and
  refreshes are not counted in the CPU runs, so the shared-memory slowdown's
  row explanation is an inference. Results are not published.

### Cache experiment: topologies, standalone benchmark, arbiter fixes (branch `realistic-timing`, phase 7c)

- **Changed:** `rv32i_experiment --topology A|B|C-ID|D-0|D-ID` builds the
  campaign's five memory systems (two memories; one memory behind the
  two-port arbiter; L1s, a shared L2, or both) with the frozen geometry
  (`campaignCache`: 8 or 64 sets, two ways, 16-byte lines, SRAM banks, burst
  fills), whole-image loading and boundary counters. `TwoPortMemoryArbiter`
  takes a burst length and keeps a granted burst to its last word, and has
  a one-entry response buffer per port: a response leaves the bus into its
  owner's buffer, so a core stalled on its other port no longer deadlocks
  the shared bus. New `CachePerformanceBenchmark` (S0–S2 standalone traces,
  in the memory benchmark runner). A clocked SRAM macro no longer gets a
  delay twin (it changed nothing but cost a cached CPU 3–40× run time). The
  generated benchmarks accept up to the data layout's 1024 words.
- **Why:** the cache plan's Step 2 and Step 3 infrastructure; the arbiter
  defects were found by the new topology test.
- **Verified:** Step 1 in both profiles (L1 hit 2, L2 hit 8, DRAM 8–22
  cycles); `RV32ITopologyTest` (new: all five topologies verify a sort, plus
  a program through D-ID over SRAM), `TwoPortMemoryArbiterTest` (rewritten,
  burst cases), cache scenario tests, `CacheReadTest`,
  `RV32IMemoryMigrationTest`, `RV32IExperimentCLITest`, the benchmark tests;
  DelayRuleTest on the SRAM parts without the twin in all three profiles;
  the standalone benchmark in `embedded` and `desktop`, every read verified.
- **Limits:** cached and arbitrated roots are not certified by the analyzer
  (no all-state contract for an endpoint behind them); they run at the
  core's certified clock, measured to be at least 37 % (`embedded`) and 43 %
  (`desktop`) above their failure point, and below that point they fail
  silently (the cache's state bits have no setup check). Open defect: the
  gate-level clocked SRAM macro corrupts a row neighbour when driven by the
  cache; compact banks (the campaign profile) are correct. The CPU panel is
  not yet run.

### Burst reads and burst-filled cache lines (branch `realistic-timing`, phase 7b)

- **Changed:** a burst read (op 4, `Operation::ReadBurst`) returns the
  `block_words` words of the aligned block from the requested word,
  wrapping, one response per word. The transaction port reports and uses
  LAST (a burst holds the port until its last word) and SPACE; the static
  sequencer steps a word address within the block (the clocked macro is
  given the next word as one is taken); the DRAM command control issues one
  READ per word in the open row (`block_words` ≤ `words_per_row`). The cache
  accepts op 4 (a hit streams the line from its data bank, a miss forwards
  the fill), has `fill=words|burst` (one op-4 request per miss), and fills a
  line from the requested word in both modes; its response still waits for
  the whole line. `block_words` = 4 has its own proven delay tables
  (`@block=4`). The safe-clock handshake model takes the multi-word states
  only for requesters that can ask for bursts. Fixed on the way: the
  transaction-port scenario's undriven LAST, the cache's top-level helper
  names (the floorplan rejected them since 7a), and MemoryEndpointTimingTest's
  pre-6.4a expectation that a request is taken over a held response.
- **Why:** word-by-word fills made an L1 miss cost about five DRAM reads (38
  cycles against an 8-cycle row hit), a protocol defect against caches. With
  bursts the cache plan's Step 1 order holds at `embedded`: L1 hit 2, L2 hit
  8, L1 miss 14 (DRAM open row 8), L1+L2 miss 17 cycles. No memory interval
  was changed.
- **Verified:** tables regenerated to a fixed point (754 entries per
  profile) and recomputed identically in all three profiles; DelayRuleTest
  on 14 memory contracts with their variants (24 parts) in `unit`,
  `embedded` and `desktop`: no rule break, no target missed; a 42-test
  CTest subset at `unit` (tables, timing, safe clock, memory endpoints,
  controllers, devices, contracts, caches); the CPU panel (66 runs, all
  verified): unchanged at `embedded` and `desktop`.
- **Limits:** the advanced core's `unit` safe clock with clocked SRAM rose
  158 → 174 ps: the burst gates on the macro's request-to-CE_N path,
  extrapolated to 1024 subarrays (recorded in the progress log); `embedded`
  and `desktop` clocks did not change. Documentation checks
  (`MemoryDeviceDocumentationTest`, three `MemoryEndpointIntegrationTest`
  checks) and CacheLayoutTest's saved layouts fail until phase 8. NOR flash
  received unused `@block=4` tables (it rejects bursts); they go at the next
  regeneration. The full suite was not run.

### Cache with directly driven banks (branch `realistic-timing`, phase 7a)

- **Changed:** `Cache<W>` looks a request up in every way at once in one
  cycle: per way a tag bank and a data bank are native devices driven
  directly (`sram` = the clocked macro, `ff` = the flip-flop device), the
  hit way's word is taken into the result register at the next edge, and the
  response is used one edge later (a two-cycle hit). A read miss writes each
  fill word into the victim's data bank as it arrives and takes the requested
  word as it passes; writes stay write-through, no-allocate. The serial
  endpoint-store phase machine is replaced (`CacheLookupPhases`,
  `CacheBankController`); bank names `TAG_STORE_k`/`DATA_STORE_k` are kept.
- **Why:** the old controller made every phase a whole endpoint transaction
  (a 14-cycle L1 hit); the timing plan's phase 7 rebuilds the hit path.
- **Verified:** cache component, direct-mapped, scalable and two-level
  scenarios, CacheReadTest and RV32IMemoryMigrationTest at `unit`; a latency
  probe at `embedded` (L1 hit 2 cycles).
- **Limits:** line fills still move word by word (L1 miss 38 cycles against
  an 8-cycle DRAM read); burst reads follow (phase 7b) before the cache
  experiment. Public cache pages and examples are not yet updated (phase 8).

### Endpoint handshake from registered state, one DRAM edge per command, core sweep (branch `realistic-timing`, phase 6.4–6.5)

- **Changed:** every memory endpoint (gate-level transaction port and FF
  controller, compact controller, fused endpoint) takes a new request from
  registered state only: REQ_READY = !RST & (!PENDING | COMPLETION) &
  BACK_READY & !FULL, no longer following the same cycle's RSP_READY. A held
  response and one request in flight can coexist. The clocked SRAM macro
  has no logic before its registers: subarrays get CE_N and WE_N ungated and
  their address with the select bits inverted, and the bus enables are
  decoded after the edge (setup at 512 words 930 → 408 ps). The DRAM device
  shows a completion as soon as its sampler sees it and takes the next
  command at that edge (one synchronizer edge per command instead of two).
  The analysis's handshake models (built-in FF endpoint, declared FF
  contract, safe-clock state enumeration with DRAM refresh phases) follow.
- **Why:** the old handshake let the core's stall logic reach the memory's
  request inputs in one cycle (clocked SRAM certified at 5,236 ps against
  4,104 now); DRAM was 38–52 % over a real controller's latency.
- **Verified:** tables at a fixed point and verified in all profiles; delay
  rule (default) 0 breaks on 53 memory parts in all profiles; CTest subset
  (endpoint safe clock, experiment and CLI, examples, arbiter, migration,
  memory state and feedback timing) passes; gate-level handshake-state checks
  pass for FF, SRAM and DRAM; 66-run panel; core sweep of 306 verified runs.
- **Limits:** DRAM still +23–33 % over a counted controller; macro 3.1×
  CACTI; STM32F7 check not done; no full suite.

### Datasheet DRAM and NOR timing and a clocked on-chip SRAM macro (branch `realistic-timing`, phase 6.3)

- **Changed (6.3a):** the timing profiles hold datasheet intervals for
  external DRAM (`embedded` SDR SDRAM -6A, `desktop` DDR4-3200: tRCD, CL, tWR,
  tRP, tRFC, tREFI, page size) and NOR flash (tACC 120 ns, program 150 µs,
  erase 410 ms). DRAM REFRESH can include the full restore; DRAM rows up to
  1,024 words are allowed for compact devices (gate-level stays at 16); the
  RV32I experiment takes DRAM row size and refresh rate from the profile.
  Functional flash endpoint scenarios set short program and erase pulses.
- **Changed (6.3b):** SRAM devices, controllers and endpoints accept
  `clocked=1`: a synchronous macro with input registers, an access token in
  place of the address-transition detector, and the same self-timed stages.
  Large arrays drive a shared return bus (`MemoryBus` leaf, READY pulled
  low) instead of a multiplexer tree; in a clocked array every subarray takes
  each access and only WE_N is gated by the select, and the clock is ideal
  (the global wire counts in the setup). The sense replica now covers the
  wordline path and the column multiplexer, so READY follows the data (also
  for the asynchronous SRAM); in clocked mode precharge covers a fully swung
  bitline. `--memory sram` is now the clocked macro and `--memory sram-async`
  the external chip. Delay tables gain `@clocked` entries (649 per profile).
- **Fixed on the way:** the timing graph marked a one-bit write mask as a
  clock carrier ("clock used as data"); `MemoryOutputBuffer` rejected one-bit
  status lines; `DelayRuleTest` stepped one tick at a time (it now steps from
  event to event, about 100× faster with picosecond ticks) and skipped the
  DRAM/NOR endpoints as restless (it now follows a part until it goes quiet).
- **Why:** owner rule that memories be realistic, not cache-favouring; the
  clocked macro is the bank the cache phase will use.
- **Verified:** tables at a fixed point and `--verify` identical in all three
  profiles; delay rule in the CTest configuration on DRAM/NOR/SRAM/ROM plus the
  clocked variants, 0 breaks in all three profiles; SRAM, endpoint, controller,
  native-device and cache tests at `unit` and the SRAM/endpoint set at
  `embedded`; a probe of 400 random accesses per size shows READY after the
  data in every read; 66 oracle-verified CPU runs.
- **Limits:** the clocked macro is 3.1× CACTI at 512 B (reported, not tuned);
  its integration makes the core–endpoint handshake longer (certified clock
  `embedded` 5,236 ps against 4,608 ps), to be decoupled in 6.4; a 30-round
  delay-rule sample reaches a known gap in the delay twin's X timing; no full
  suite; CacheReadTest and the other tick-driven tests stay `unit`-only.

## 2026-10-16

### Memory controller protocols closer to real controllers (branch `realistic-timing`, phase 6.1–6.2)

- **Changed:** DRAM and NOR devices take the next command during the
  releasing cycle (BUSY shown low there), with FRESH and REPEAT state so a
  stale sample or a repeated command cannot complete early; the command
  sequencer shows the next command combinationally when the device is idle
  and fault-free. SRAM/ROM controllers count `access_cycles` (new endpoint
  parameter, chosen from the proven access time at the clock by
  `staticAccessCycles`) instead of synchronizing READY; READY is checked at
  the capturing edge (status Device if not high).
- **Why:** owner rule that memories be realistic, not cache-favouring: the
  removed cycles (about three per DRAM/NOR command, two per SRAM/ROM
  access) are not spent by real controllers.
- **Verified:** DRAM/NOR/SRAM/ROM device, controller, endpoint and cache
  scenarios (with a new back-to-back command case); tables at a fixed point
  in all profiles; delay rule under `unit` and `embedded`; 54
  oracle-verified CPU runs at re-certified clocks.
- **Limits:** realistic memory intervals still placeholders (6.3); no full
  suite.

### On-chip SRAM geometry and READY after data (branch `realistic-timing`, phase 5)

- **Changed:** SRAM and ROM devices raise READY only after the read data are
  valid (an extra timer stage copying the sense and output path). A gate-level
  SRAM above 32 rows is built from 32-row subarrays with a global decoder,
  global wires (new `MemoryWire` leaf, `sram_wire_per_side` in the profile)
  and a return multiplexer and output buffer; the contract is unchanged. SRAM
  tables are proven up to 512 words; larger SRAMs use `sramSubarrayEstimate`
  (per-level growth fitted at 2 and 4 subarrays, wire term from the profile).
  The compact SRAM withdraws its data no sooner than the gates' path for that
  transition. New tests: `SRAMSubarrayTest`, `SRAMSubarrayFormulaTest`.
- **Why:** realistic on-chip arrays are divided into short-bitline subarrays;
  cache banks driven directly need READY after data; the delay rule under the
  realistic profile found the early withdrawal.
- **Verified:** held-out 8-subarray tables matched exactly in all three
  profiles; SRAM/ROM/endpoint/cache scenarios under `unit` and `embedded`;
  delay rule for SRAM/ROM parts under all profiles; tables verified.
- **Limits:** `SRAMTimingTest` remains a `unit` test (tick-driven); realistic
  memory intervals are still placeholders; SRAM CPU cycles rose with
  READY-after-data; no full suite.

### Memory timing under realistic profiles (branch `realistic-timing`, phase 4.4)

- **Changed:** the SRAM/ROM timer's settle line is sized from its own
  change-detection path (`memoryTimerSettle`); DRAM sense and precharge steps
  wait for a row-decoder replica (`dramDecodeReplica`) in the gates and the
  cycle-exact copy; compact and fused DRAM models keep storage as the row's
  logical contents while a row is open; DRAM retention defaults are timing
  profile values (64 ms in the realistic profiles, `unit` unchanged) with the
  limit widened to 10^12 ticks; a compact part whose gate list contradicts a
  change is timed by its table; delay tables regenerated for all profiles.
- **Why:** the realistic profiles exposed races hidden by one-tick gates
  (lost change pulse, sensing before the wordline, precharge before the
  wordline close) and a 100 ns retention left over from tick-era defaults.
- **Verified:** tables at a fixed point and `--verify` identical for all three
  profiles; 18 memory scenarios each under `embedded` and `desktop` and the
  affected `unit` scenarios pass; full `DelayRuleTest` passes; 54
  oracle-verified CPU runs (five memories, three profiles).
- **Limits:** realistic memory intervals are placeholders until phase 6;
  tick-driven timing tests remain `unit` tests; no full suite.

### Gate-level memory control and self-timed memory devices (branch `realistic-timing`, phase 4.1–4.3)

- **Changed:** the memory controllers' sequencing is now gates. SRAM and ROM
  use `StaticMemorySequencer` (request check, registers, READY through a
  two-stage synchronizer); DRAM and NOR flash use `CommandMemorySequencer`
  (pending command list, one-cycle ISSUE, wait for the device's registered
  BUSY, open-row tracking, a cycle-counted refresh). The C++
  `NativeSequencer` primitive is removed. The devices time themselves: SRAM
  and ROM raise READY from replica timing (`MemoryTimer`, delay lines); DRAM
  and NOR command controls are synchronous gates with analog delay-line
  chains and a sampler. New timing leaves: `MemoryDelayLine`,
  `SymbolicDiffer`, `Synchronizer`, `ProtocolStableData`, `KnownBits`,
  `PowerOnReset`. Compact controllers, compact devices and the fused endpoint
  are cycle-exact copies (`NativeSequence`, `DRAMSyncControl`,
  `NORSyncControl`) timed by tables proven on the gates. DRAM refresh is
  configured in cycles (`refresh_interval_cycles`, default 96; 0 or at least
  24). The device's open row is visible as ROW_OPEN.
- **Why:** decision 2A of the realistic-timing plan (devices time themselves
  and signal completion) and the lower-level-first rule; tick timers in C++
  could not follow realistic gate delays.
- **Verified (unit profile):** focused memory scenarios (device, contract,
  timing, controller, all endpoints, scalable endpoint, cache scenarios) pass
  with every fidelity compared; `DelayRuleTest` on SRAM/ROM/DRAM/NOR devices,
  controllers and endpoints: no rule break; oracle-verified CPU runs, e.g.
  program 21 at 696 (SRAM), 1,375 (DRAM) and 732 (NOR) cycles; safe clock
  102 (SRAM/ROM), 106 (DRAM), 104 (NOR) ticks. Details in the progress record.
- **Limits:** cycle counts with timed memories grew (synchronizer and
  clocked command protocol); published results remain reproducible at tag
  `timing-legacy`. Realistic-profile tables for the memory parts are stale
  until step 4.4. `MemoryEndpointIntegrationTest` fails its two C++ example
  checks because they compile reference models from `site-docs/` snippets
  that still describe the old sequencing; the public site is updated in
  phase 8 by owner request. No full suite.

### Timing profiles, inertial gate delay and realistic gate delays (branch `realistic-timing`)

- **Changed:** leaf parts (logic gates, splitter, memory primitives) read
  their delays from a process-wide timing profile
  (`include/components/TimingProfile.hpp`), selected by
  `CIRCUITSIM_TIMING_PROFILE`, the Python `set_timing_profile` or code.
  Profiles: `unit` (default; unchanged timing), `embedded` (65 nm, FO4 30 ps)
  and `desktop` (32 nm, FO4 21 ps) with 1 tick = 1 ps and sourced gate ratios.
  Logic gates use inertial delay (pulses shorter than the delay vanish).
  Flip-flop and SRAM-cell contracts are measured per profile and stored in
  it. Delay tables are generated per profile, each in its own source file;
  `part_delay_tables --profile` and two CTest verify entries were added.
- **Why:** the owner decided to give leaf parts realistic delays while keeping
  behavioral parts derived from their gate forms (see the realistic-timing
  plan and progress records).
- **Verified:** under `unit`, `part_delay_tables --verify` identical (601
  entries) and nine CPU runs identical in every field but host time against
  the baseline binary, with and without inertial delay; 23 scenarios passed.
  Under `embedded`/`desktop`: tables verified at a fixed point, flip-flop and
  SRAM-cell tests passed, all three cores certified and ran programs with
  identical cycles and oracle checks in compact and gate-level form;
  `DelayRuleTest` sample passed.
- **Limits:** no full suite. Timed memories (SRAM/DRAM/ROM/NOR) keep
  tick-based timers and are not valid under realistic profiles until the
  gate-level memory control phases. Scenario tests stay on `unit`. The public
  site is unchanged by owner request.

### Retire the redundant memory container and document test workflows

- **Changed:** scalable endpoints now select each technology's behavioral
  native device under the existing device contract while keeping the
  controller visible. Retired container contract IDs map to native devices.
  Updated memory pages, diagrams, visualizer aliases and examples. Added a
  test and scenario workflow guide linked from AGENTS.md.
- **Why:** the container only reselected and relabeled an existing behavioral
  device; removing it leaves one replaceable native-device contract per
  technology. The guide makes registry, CTest, scenario and site checks easier
  to repeat when components change.
- **Checked:** serial native build, 28 focused CTests, 601 delay-table entries
  verified, diagram and JSON checks, full local Pages/WASM package with strict
  MkDocs, and 7/7 focused browser/WASM parity checks passed.
- **Limits:** the full CTest suite and published URL were not checked. These
  memory and guide changes remain uncommitted in the local working tree.

### Trigger Pages deployment from alpha

- **Changed:** the Pages workflow push trigger now listens to `alpha`; the
  workflow-only change was pushed to `origin/alpha`.
- **Why:** `alpha` is the selected source branch, and no `github-pages` source
  branch exists.
- **Checked:** GitHub Actions reported a run in progress for the pushed
  trigger commit. The full local site package and seven focused web checks
  passed after the push.
- **Limits:** the user directed us to treat publication as done while the
  remote run continued; its final result and live URL were not verified. The
  local memory and guide changes were not in that pushed commit.

### Fix one technology stack for the cache experiments

- **Changed:** replaced the broad FF/SRAM/DRAM performance sweeps in the open
  cache record with a complete plan for SRAM L1 and L2 caches over DRAM main
  memory. Fixed the initial geometry, small structural/scalable check, three
  standalone paths, five CPU paths, workloads, counters and completion gates;
  updated status.
- **Why:** one consistent technology stack tests the intended cache hierarchy
  without multiplying the campaign by technologies. DRAM is backing memory;
  the cache's tag/data banks currently support SRAM or FF, not DRAM.
- **Checked:** reviewed cache and endpoint source, CPU root and experiment
  runner, benchmark tooling and current documentation; `git diff --check`
  passed. No benchmark or simulation was run for this planning change.
- **Limits:** SRAM cache-hit timing, shared CPU bypass/L2 wiring, benchmarks
  and physical timing calibration remain open. The plan does not claim speedup.

### Focus the planned CPU cache campaign

- **Changed:** reduced the primary CPU matrix from nine variants to five:
  separate memories, shared uncached memory, both L1s, L2 alone, and both
  L1s over L2. Kept I-only/D-only runs as targeted follow-ups; updated the
  open cache record and status.
- **Why:** the five cases answer the main sharing, L1 and L2 questions while
  avoiding duplicate `C-0`/shared-uncached runs and a broad ablation sweep.
- **Checked:** reviewed the existing topology plan and cache contract;
  `git diff --check` passed. No simulation was run for this planning change.
- **Limits:** the CPU bypass, shared L2 and benchmark campaign remain to be
  implemented.

## 2026-10-15

### Refine the advanced-core cache comparison matrix

- **Changed:** specified nine CPU topologies in the open cache record:
  separate memories, shared uncached memory, I/D L1 combinations, and the
  same combinations over a shared L2. Added comparisons, workload choices,
  counters, correctness gates and implementation order; updated status.
- **Why:** one-level and two-level results need shared-memory controls, and
  instruction/data cache effects need independent switches.
- **Checked:** reviewed current CPU root/runner wiring, cache and arbiter
  contracts, benchmark workload catalogue and memory-wait reference;
  `git diff --check` passed after the documentation edit.
- **Limits:** plan only. The shared uncached bypass, CPU-side L2 path and
  nine-topology campaign have not been implemented or run.

### Specify matched cache experiments

- **Changed:** added an executable experiment protocol to the open cache
  record and updated status. It defines direct-versus-cache, two-level and
  advanced-core pairs, workloads, measured boundaries and result gates.
- **Why:** cache traffic savings, modeled latency, CPU cycles and simulator
  host cost need separate evidence under matched backing-memory conditions.
- **Checked:** inspected the cache and memory contracts, current scenario and
  benchmark runners, CPU topology construction and timing documentation;
  `git diff --check` passed. No performance experiment was run.
- **Limits:** this is a plan. No benchmark fixture, timing model or CPU path
  was changed, and no direct-versus-cached performance run was made.

### Recognize existing memory delay comparisons in the cache plan

- **Changed:** corrected the open cache record and status to account for
  `DelayRuleTest` and its gate-versus-compact memory timing comparisons.
- **Why:** the prior timing review overlooked these tests and made a new
  comparison phase sound necessary before exploratory cache experiments.
- **Checked:** a focused run of five 8-bit endpoint contracts and an FF
  variant completed 3,208 situations, with 970 paired output changes and no
  delay-rule break or unrecorded closeness failure.
- **Limits:** the self-timed DRAM endpoint yielded no paired output changes
  in this focused run; exact transaction latency under every request and
  physical hardware timing remain unverified. No cache speedup was measured.

### Review memory realism before cache performance comparisons

- **Changed:** added a current-source audit and staged comparison design to
  the open cache record; clarified the outstanding timing work in status.
- **Why:** structural/behavioral agreement establishes digital model fidelity,
  while phase choices, controller waits and large-depth estimates still need
  separate physical plausibility checks before interpreting cache speedups.
- **Checked:** eight focused memory contract and timing CTests passed in the
  existing `build-cleanup` binary; inspected cache, native-device, endpoint
  and delay-table source plus the repository textbook and vendor references.
- **Limits:** no code or timing model was changed, the test binary was not
  rebuilt for this review, and no direct-versus-cached latency comparison was
  run. Vendor specifications do not calibrate simulator ticks by themselves.

### Explain cache parameters through four runnable configurations

- **Changed:** rewrote the cache page's configuration and behavior text within
  its component template, preserving the C++ reference. Added direct-mapped
  and scalable single-cache scenarios plus a connected two-level FF-backed
  scenario, with menu aliases, saved layouts and browser checks. Added an
  opt-in precomputed-playback hook so the two-level layout can be built
  without simulating its transactions, and documented that contract in the
  test guide and reference.
- **Why:** the original page offered one visual configuration and did not
  clearly explain word width, sets, ways, line size or backing range.
- **Checked:** eight focused native CTests passed; the strict Pages/WASM build
  generated 30, 116 and 58 verified rows for the single-cache examples; the
  focused memory browser test passed 1/1, including all four cache scenarios.
  `CacheDocumentationTest` still compiles the unchanged C++ reference.
- **Limits:** the first three examples script their lower port. The fourth
  uses one requester and broadcasts invalidation only while both caches are
  idle. The full native and browser suites were not rerun; these scenarios do
  not establish cache speedup or coherence.

### Broaden the cache scenario and verify a two-level cache path

- **Changed:** expanded the visual component scenario to two sets, two ways
  and two-word lines, covering spatial hits, LRU replacement, writes,
  invalidation and range rejection. Added a headless upper-cache/lower-cache
  fixture over FF RAM, including idle-time invalidation of both levels;
  updated the saved layout and public page.
- **Why:** the original one-line read-only scenario could not demonstrate the
  cache geometry or whether cache components compose into levels.
- **Checked:** native scenario and broad cache test passed; focused CTests
  passed 3/3; the Pages build generated 116 WASM-verified cache rows and
  passed strict MkDocs; the memory endpoint browser suite passed against
  port 8893, including the two-way cache layout.
- **Limits:** the stack is a functional single-requester path. CPU I/D
  arbitration, MMIO bypass, coordinated invalidation, coherence and clock
  measurements remain open. The first browser attempt used a venv without
  Playwright; the installed system Python completed the test.

### Replace the cache construction snippet with a behavioral reference

- **Changed:** the public cache page now gives a compilable C++ reference for
  lookup, full-line refill, two-way replacement, masked write-through and
  invalidation. Added a documentation CTest that extracts the exact block.
- **Why:** neighboring component pages use C++ examples to explain behavior;
  merely constructing a cache did not explain its algorithm.
- **Checked:** the new CTest passed 1/1; the strict MkDocs build passed with
  the current generated cache rows; `git diff --check` passed.
- **Limits:** the reference covers successful settled binary transactions,
  not clocked handshakes, four-state behavior or failure statuses. It is not
  a registered behavioral cache implementation.

### Make the cache scenario runnable in the local WebAssembly visualizer

- **Changed:** activated the cached Emscripten SDK, rebuilt the Pages
  simulator, and regenerated the cache example from its WASM timeline.
  Extended the memory browser smoke check to include the cache page and a
  cache-specific layout assertion. Clarified the separate tag/data stores
  in the public reference.
- **Why:** the new cache scenario existed in native tests but the previous
  local browser package could not load it.
- **Checked:** serial WASM build, layout packaging, 12 verified cache rows,
  strict Pages build, and focused `MemoryEndpointBrowserTest` passed. Port
  8893 serves the new scenario JSON and the byte-identical rebuilt WASM
  binary; its cache page returned HTTP 200. `git diff --check` passed.
- **Limits:** the whole browser smoke suite and GitHub Pages deployment were
  not run; there is still no fused behavioral cache or RV32I integration.

### Restore the local Pages preview after adding the cache example

- **Changed:** placed the native-verified 12-row cache export into the
  ignored local example bundle used by the port 8893 MkDocs service. Recorded
  the outage and clarified the cache page's test and fidelity scope.
- **Why:** the new page requested `cache-read-hit`, but the preview's older
  generated bundle lacked that scenario; MkDocs aborted its rebuild and
  left the service unresponsive.
- **Checked:** the preview's strict MkDocs build passed; its home, cache and
  simulator pages returned HTTP 200. Both cache tests passed 2/2, including
  the headless broad test and the recorded component scenario.
- **Limits:** at that recovery step, the simulator package still predated
  the cache scenario. The subsequent Pages rebuild above replaced it.

### Add endpoint-backed structural and scalable-storage caches

- **Changed:** added `Cache<W>` for 8/16/32-bit words with structural tag
  lookup, one/two-way replacement, one/two/four-word fills, all-line
  invalidation, and optional masked write-through/no-write-allocate. Tag and
  data banks independently use existing FF or SRAM endpoints. A second
  catalog form selects scalable endpoint storage while retaining the same
  public pins and structural controller. Added a registered cold-miss/hit
  component scenario, visualizer alias and saved layout. Rewrote the public
  cache reference in the component guide's format, with a generated example.
- **Why:** build a reusable cache from the existing transaction endpoint
  contract and memory technologies before wiring one into RV32I.
- **Checked:** serial `SimulationTestRunner` build; six focused cache,
  catalog and profile CTests passed. Cache checks include a deterministic
  independent set/way reference stream and partial-fill fault. The new
  component scenario passed with 12 checkpoints. A final focused CTest run
  passed 4/4, including visualizer catalog (8/8) against the rebuilt native
  backend. Native-generated example rows passed a strict MkDocs build and
  appeared in the rendered page. The C++ page example compiled; `git diff
  --check` passed.
- **Limits:** geometry is explicitly bounded, the standalone fixture does
  not cover every four-state/fault timing path, and cache timing and
  advanced-core integration have not been measured. Shared RAM and
  arbitration remain open in the cache record. The initial Pages attempt
  lacked an active `emcmake`; the subsequent rebuild above verified the
  local browser scenario. The published site has not been checked.

## 2026-10-14

### Define cache geometry as construction parameters

- **Changed:** refined the proposed `Cache<W>` interface: W selects the
  supported endpoint word-width specialization; textbook sets, ways and
  block bytes become fixed construction parameters; address width remains
  32 bits under the current endpoint contract. Derive words per line from
  block bytes and W instead of exposing both values independently.
- **Why:** the parameters must shape a structural circuit, while the existing
  component catalog needs to select geometry without a C++ type for every
  combination.
- **Checked:** current memory width templates, construction configuration,
  endpoint catalog parameters and the local textbook's cache organization;
  edited Markdown links and whitespace.
- **Limits:** design clarification only; no cache code, build or test ran.

### Generalize the proposed cache across endpoint word widths

- **Changed:** corrected the cache plan from a 32-bit-only component to one
  width-parameterized structural blueprint with 8, 16 and 32-bit contracts.
  Distinguished 32-bit byte addresses from data-word width, kept 32-bit tag
  endpoints, and added width and geometry proof gates.
- **Why:** the existing memory endpoints already generalize word width; a
  reusable cache should match all supported endpoint widths.
- **Checked:** endpoint catalog registrations, parameter validation, REQ/RSP
  pin schema, transaction frontend and storage widths; edited Markdown links
  and whitespace.
- **Limits:** design correction only; no cache code or width-parity test ran.

### Specify cache components and endpoint wiring

- **Changed:** added an ordered inventory for the proposed `Cache32`
  assembly, structural tag matcher, line state, controller, reused tag/data
  endpoints and standalone test root. Clarified exact REQ/RSP port
  compatibility and the distinct cache-control responsibility.
- **Why:** memory endpoints supply replaceable storage, while a cache must
  still map addresses, track valid tags and coordinate refill transactions.
- **Checked:** current endpoint pin schema and assembly, transaction frontend,
  gate/splitter/register blocks and an existing structural tag comparator;
  edited Markdown links and whitespace.
- **Limits:** planning only; no code, compile or simulation was run.

### Require geometry evidence for the cache plan

- **Changed:** clarified how the textbook's set, way, block-size and address
  parameters determine the structural circuit. Added one- and two-set checks
  to the first read-cache gate, followed by separate block-size and way checks.
- **Why:** a single four-set example cannot demonstrate a general cache.
- **Checked:** current cache record and local textbook chapter 6; edited
  Markdown links and whitespace.
- **Limits:** this is a planning correction; no cache implementation or
  geometry test was run.

### Scope the first structural-cache deliverable

- **Changed:** specified a standalone four-set, one-way, one-word read-cache
  slice, its new structural logic, reused endpoint children, a concrete
  hit/conflict trace and an internal endpoint replacement check.
- **Why:** the first implementation step now has an observable finish line
  while keeping memory technologies selectable.
- **Checked:** current cache record, memory protocol, endpoint classes,
  transaction frontend, component catalog and structural wiring; edited
  Markdown links and whitespace.
- **Limits:** proposed scope only; no cache code or simulator test was run.

### Detail the standalone structural-cache plan

- **Changed:** expanded the open cache record with a textbook-grounded cache
  organization, one proposed public cache contract, independently replaceable
  tag/data endpoint banks, a visible lookup/refill/write sequence and staged
  verification gates. Updated living status.
- **Why:** the first structural cache needs a concrete general interface and
  replaceability tests before implementation.
- **Checked:** local textbook chapter 6, current memory protocol and endpoint
  assembly, structural wiring, component profiles and endpoint scenarios;
  checked edited Markdown links and whitespace.
- **Limits:** this is design work. No cache code, simulation, benchmark or
  public component page changed; proposed policies and timing are unverified.

### Add seven-target RV32I result figures to §4.7

- **Changed:** added a measured FF workload-group overview, controlled
  dependency/load-scheduling contrasts, a longer-branch matrix and a separate
  conditional full Embench projection. Each core comparison displays
  single-cycle, baseline and all five advanced prediction modes. Added a
  reproducible chart generator and instructions.
- **Why:** the replacement figures make the three-core comparison visible
  while keeping narrow experiments and historical projections in their
  proper scope.
- **Checked:** validated all 602 public result-table cells against completed
  inputs and the 133 projected Embench cycles against the original historical
  cases; inspected PNG previews and SVG metadata; confirmed four built SVGs
  match the source assets; passed two focused documentation CTests, strict
  MkDocs build and `git diff --check`.
- **Limits:** no benchmark rerun or simulator change. Full Embench remains a
  clock-only projection from older cycles; no full native suite, WebAssembly
  package or browser check was run.

### Rewrite the component creation guide for the current workflow

- **Changed:** replaced the deprecated checklist with the current path for
  structural construction, catalog registration, scenario and equivalence
  tests, generated behavioral delays, frontend registration and the public
  component page template. Corrected linked timing commands to preserve
  clocked tables and use complete verification.
- **Why:** the older instructions omitted the proven-delay step and treated
  optional direct Python bindings as mandatory.
- **Checked:** traced the instructions through source and build scripts;
  37 local links and 12 source example paths resolved; `git diff --check`
  passed.
- **Limits:** no component code or public page changed, so C++ tests, delay
  generation, the site build and browser checks were not run.

### Withdraw §4.7 charts and replan the visual comparisons

- **Changed:** removed all four §4.7 SVG charts, their page embeds and the
  chart-only generator. Recorded a new figure plan grounded in the current
  §4.2 test groups and §4.7 result tables; each proposed core comparison
  shows all seven comparable FF targets.
- **Why:** the previous core and Embench plots hid advanced/two-bit as an
  implicit denominator and displayed only two core series; other plots had
  narrower scopes than the report's central three-core question.
- **Checked:** reviewed the current experiment setup, report and source
  results; confirmed the rendered page has no chart embeds or remaining
  §4.7 assets; passed `RV32IBenchmarkDocumentationTest` (1/1), strict MkDocs
  build and `git diff --check`.
- **Limits:** no replacement chart or benchmark run was made. The full
  Embench figures remain conditional projections from older cycles.

### Correct cache-storage replaceability at the endpoint boundary

- **Changed:** corrected the open cache record and status: tag/data banks can
  be built from existing transaction endpoints and later swapped for scalable
  endpoint forms without a native FF/SRAM pin adapter. Native array wiring is
  now an optional optimization.
- **Why:** the existing endpoint interface was designed to make memory
  technologies interchangeable for requesters, including a cache controller.
- **Checked:** current `protocolPins`, endpoint catalog and structural
  controller/device assembly; edited Markdown links and whitespace.
- **Limits:** replaceability preserves pins and handshake, while operation
  support and latency vary by technology. No cache was built or benchmarked.

### Rewrite §4.7 around the §4.2 workload groups

- **Changed:** reorganized the complete results report into fixed programs,
  generated families, controlled core-gap cases, compiled fixtures and a
  separately marked full Embench projection. Placed each result table beside
  its interpretation, split core-gap settings by mechanism, and made the
  seven-target tables show modeled ticks with a clear clock conversion.
- **Why:** the earlier analysis and tables were far apart, making it hard to
  follow how the three cores and five advanced branch policies performed on
  each test group.
- **Checked:** validated all 99 displayed result rows against the previous
  report and complete summaries; recomputed winner counts and ratios;
  passed two focused documentation CTests, the strict MkDocs build, chart
  link/anchor checks and `git diff --check`.
- **Limits:** no benchmark or code change. Full Embench is still a
  conditional clock-only projection; no full native suite, WebAssembly
  package or browser check was run.

### Assess reuse of existing memory components inside caches

- **Changed:** added a cache-record feasibility review of reusable FF/SRAM
  arrays, scalable native storage, and technology-selectable backing endpoints.
  Clarified the proposed structural-controller-plus-scalable-array path.
- **Why:** the owner asked whether cache construction can follow the endpoint
  composition pattern without rebuilding memory devices.
- **Checked:** current endpoint assembly and catalog, native FF/SRAM pins,
  memory protocol, structural size limits, and primary Ibex/AMD cache
  descriptions; edited Markdown links and whitespace.
- **Limits:** no cache was assembled or timed. FF/SRAM cache-array adaptation,
  policy compatibility and hit-path speed remain implementation questions.

### Reorder the cache plan around a standalone structural component

- **Changed:** revised the open cache and future-steps records and living
  status to adopt structural cache, measured/timed unified scalable RAM, then
  equivalent scalable caches. Added component contracts, latency measurement,
  shared-memory integration and differential-test gates.
- **Why:** the owner chose this sequence and requires structural/scalable
  compatibility like the existing memory endpoints.
- **Checked:** inspected current memory and RV32I wiring, endpoint tests,
  timing-table extrapolation, benchmark scope, and the public memory and
  advanced-core contracts; checked the RISC-V instruction-fetch rule and a
  modern split-cache hierarchy against primary documentation; checked edited
  Markdown links and whitespace.
- **Limits:** this is a plan. No cache, shared-RAM root, timing calibration,
  benchmark or simulator behavior changed; the selected cache policy and
  timing model remain proposals.

### Expand §4.7 result charts and keep run provenance internal

- **Changed:** added predictor-pattern, timed-memory and conditional Embench
  projection charts to the existing core comparison; removed source/binary
  hashes and direct experiment artifact paths from public §4.7. The internal
  [results presentation record](records/2026-10-14-rv32i-results-presentation.md)
  retains their provenance and chart-generation method.
- **Why:** the single core chart did not visualize policy sensitivity, memory
  wait costs or the dense projected Embench comparison. Run-file details
  belong with internal evidence rather than in the public results narrative.
- **Checked:** regenerated four SVGs with validated source rows, inspected
  chart previews and XML, passed the focused documentation CTest and strict
  MkDocs build, confirmed chart links and built SVG copies, and passed
  `git diff --check`.
- **Limits:** no benchmark rerun or simulator change; Embench remains a
  conditional clock-only projection. No full native suite, WebAssembly
  package or browser check was run.

### Focus §4.7 on current results and performance comparison

- **Changed:** moved the former public clock-development analysis to the
  internal archive, removed it from the MkDocs navigation and §4.2 links,
  and added a dedicated §4.7 analysis of matched core, predictor and memory
  results with a reproducible SVG chart.
- **Why:** §4.7 should compare the current completed experiments without
  presenting retired timing work as part of the public results chapter.
- **Checked:** recomputed chart ratios from complete summaries with matching
  source/binary fingerprints, visually inspected the plot, and confirmed the
  archived page is absent from the built site. The focused documentation
  CTest, strict MkDocs build and `git diff --check` passed.
- **Limits:** the full Embench table remains an explicitly conditional
  clock-only projection; no new performance runs, full native suite,
  WebAssembly package or browser checks were performed.

### Assess the shared-RAM arbiter choice

- **Changed:** added a comparison of a scoped 2:1 arbiter, true two-port RAM
  and banked RAM to the open cache plan; clarified that the proposed arbiter
  serves the shared backing port while L1 hits remain independent.
- **Why:** the owner asked whether arbitration is a sound choice and whether
  modern memory systems use it.
- **Checked:** current core wiring, transaction contract and registered tests;
  AMD interconnect/cache documentation and Chipyard's memory hierarchy;
  edited Markdown links and whitespace.
- **Limits:** the recommendation is provisional; no arbitration wait or
  cache performance measurement was made, and no code was changed.

### Start the cache system plan

- **Changed:** recorded the owner's cache → game-program → AI-accelerator
  order, opened a cache topic record with proposed shared-RAM, structural
  cache, scalable-cache and measurement gates, and linked it from status.
- **Why:** the cache stage needs an inspectable contract and acceptance gates
  before implementation or game-program work begins.
- **Checked:** reviewed current memory/core wiring, transaction and wait
  contracts, registered tests, profile rules and the open future plan;
  checked the official RISC-V `FENCE.I` rule. Checked edited Markdown for
  whitespace and links.
- **Limits:** architecture and policies are proposals; no cache, unified RAM,
  program run, site build or timing measurement was completed.

### Correct behavioral-delay documentation

- **Changed:** replaced stale one-tick descriptions across ALU8, FF memory
  and RV32I component references with the catalog's gate-derived delay-table
  and optional delay-twin behavior. Updated the timing guide/reference, source
  contract comment and RV32I runner README to distinguish variable
  propagation from fixed certified periods (FF 166/80/100; timed 102).
- **Why:** the old prose described the constructor fallback as the normal
  behavioral timing model and still quoted retired clock certificates.
- **Checked:** source paths and generated table coverage; 15/15
  documentation CTests; strict MkDocs build with existing example rows;
  `git diff --check`. A direct advanced/SRAM clock query returned 102 ticks.
- **Limits:** no runtime behavior changed; the full native suite, WebAssembly
  package and browser checks were not rerun. Large-memory delay extensions
  are estimates, and a full Embench current-source run remains a projection
  in §4.7.

### Research source programs for the first guest games

- **Changed:** added a candidate sequence and direct source references for
  2048, Snake, Pong, Tetris, a later sprite game, Mandelbrot, and DOOM to the
  open future plan; no first game was selected.
- **Why:** available source can shorten a CPU-game port only after its host
  input, timing, display and runtime dependencies are identified.
- **Checked:** inspected upstream source/README/license pages and the current
  platform limits recorded in this repository.
- **Limits:** no source was downloaded into the repository, compiled for the
  guest, or run on the simulator; runtime and memory feasibility remain open.

### Add a pre-game ISA expansion decision to the future plan

- **Changed:** recorded RV32I, Zmmul, and RV32IM options and a provisional
  RV32IM recommendation for the compiled-game target; the ISA choice remains
  open, and memory/cache work stays first.
- **Why:** the owner wants to consider adding useful CPU operations, not just
  rely on software arithmetic.
- **Checked:** reviewed current decode/ALU and the RISC-V M specification;
  compiled small multiply/divide C functions with all three `-march` choices.
  The pinned compiler emitted helper calls for RV32I and Zmmul, and hardware
  instructions for RV32IM. Tiny ELFs linked, but the Zmmul image pulled in
  compressed instructions from the default runtime; RV32IM selected its
  dedicated multilib and emitted `mul`.
- **Limits:** no simulator run, hardware design, or game performance
  measurement was made.

### Record memory-first priority and RV32I arithmetic feasibility

- **Changed:** recorded the owner's decision to finish unified-memory/cache
  work before games, superseding the earlier game-first proposal; added a
  source-based assessment of software multiplication on RV32I.
- **Why:** the implementation order is now owner-directed, and hardware MUL
  is a performance option rather than a functional prerequisite for simple
  integer games.
- **Checked:** read the current ISA/toolchain contracts and official RISC-V
  and GCC references; inspected a built RV32I matrix-multiply ELF whose
  `__mulsi3` helper uses add/shift instructions.
- **Limits:** no new build, simulation, or game performance check was run;
  cache design and any M/Zmmul extension remain open.

### Sequence program and memory milestones

- **Changed:** proposed a stable address/MMIO contract, one playable CPU
  program on existing memories, shared RAM on the wait-capable core, then
  optional caches tested against slower backing memory.
- **Why:** the first game does not need shared code/data storage, while DMA
  integration does; today's zero-wait FF endpoint supplies no modeled cache
  miss penalty to reduce.
- **Checked:** reviewed current machine, memory-wait and scalable-memory
  contracts and the open plan; checked Chipyard's hierarchy reference.
- **Limits:** sequencing is a proposal; no new implementation, game run, or
  cache-speedup measurement was performed.

### Propose a shared-memory integration path

- **Changed:** recorded a preferred first merge: retain the two CPU request
  paths, arbitrate them onto one shared RAM/address map on the advanced core,
  then add optional caches after the uncached contract works.
- **Why:** one RAM image removes instruction/data divergence while the
  advanced core can tolerate contention from a single-ported backing.
- **Checked:** compared current core-memory wiring and wait contracts with
  Intel cache-hierarchy and RISC-V instruction-visibility references.
- **Limits:** planning only; no arbiter, shared RAM, or new test was built.

### Clarify scalable-memory access timing

- **Changed:** added a source-based timing comparison of the five scalable
  native devices and their transaction endpoints to the open future plan.
- **Why:** cache and shared-memory proposals depend on whether the existing
  backing memory models an address-dependent or variable access cost.
- **Checked:** inspected container, device, controller, sparse-storage and
  delay-table code; four focused memory contract/timing CTests passed using
  the existing `build-cleanup` binary.
- **Limits:** no fresh build or per-address latency sweep; large-depth FF
  delay tables are estimates, and host runtime was not measured.

### Report current RV32I performance panels at final periods

- **Changed:** replaced §4.7's older result tables with 497 completed
  current-source panel case/target rows, core/predictor/memory comparisons,
  and a separately labeled conditional 133-cell full Embench clock-only
  projection. Updated §4.2 links to the new result scope.
- **Why:** the earlier page mixed source snapshots and could present old ticks
  as current results; the full Embench workload was intentionally left as a
  projection rather than rerun.
- **Checked:** current tables were rendered from completed local summaries
  and focused direct-run JSON pairs; 420 broad FF cases, 35 long branches,
  18 memory pairs, seven focused program-20 memory pairs, three capacity
  cases and 14 ELF fixtures passed their recorded checks. The focused
  documentation CTest, strict MkDocs build and `git diff --check` passed.
  The non-Embench native suite had 449/450 sandbox passes and its sole
  sandbox `EPERM` case passed separately outside the sandbox.
- **Limits:** projected Embench ticks assume unchanged historical cycles;
  no current-source full or reduced Embench suite was run. The site build
  reused generated examples; WebAssembly packaging and browser checks were
  not run.

### Evaluate unified memory and cache integration

- **Changed:** added a source-based comparison of the current dual-memory
  testbench, unified backing-memory options, and optional small/scalable
  cache stages to the open future-steps plan.
- **Why:** device I/O and future DMA need one address-map decision, while cache
  misses and coherence would change current core and timing contracts.
- **Checked:** traced memory wiring, endpoint storage, ELF loading, oracle
  modes, core timing limits and public contracts; checked primary RISC-V and
  Chipyard references. No build or test was run for this planning increment.
- **Limits:** no shared-memory/cache prototype or measured cache benefit;
  design and integration order remain proposals.

### Assess interactive platform, GPU, and AI accelerator goals

- **Changed:** recorded current implementation gaps, external architecture
  references, and candidate milestones in the open future-steps plan.
- **Why:** the three requested goals need distinct success criteria and shared
  device/memory groundwork before implementation choices are made.
- **Checked:** read current simulator, RV32I, memory, visualizer and test
  sources, open records, and primary external references; `ctest -N` lists
  451 tests, but no build or test was run for this planning review.
- **Limits:** no game or accelerator prototype, throughput measurement, or
  approved design; feasibility and sequence are proposals.

### Compare the Embench core results

- **Changed:** recorded per-core full Embench winners, a fixed-policy
  modeled-time comparison, reduced-work structural evidence and the boundary
  between measured old periods and a conditional final-clock projection.
- **Why:** the 19-workload suite gives broader performance evidence than the
  current short branch and program-20 cases, but it ran on an older circuit.
- **Checked:** audited all 133 completed full case summaries and nine reduced
  structural summaries; recomputed periods, winners and geometric mean ratios.
- **Limits:** artifact analysis only; no current-source Embench run, new
  simulation, CTest or site build.

### Analyze performance of the three RV32I cores

- **Changed:** recorded the three-core modeled-time comparison, clock/cycle
  break-even thresholds, predictor effects and historical counterexamples.
- **Why:** a shorter certified period does not by itself determine which core
  finishes a workload first, and historical broad rankings are not current
  measurements.
- **Checked:** recalculated five current branch comparisons and current
  program-20 times from artifacts; counted winners across the 133 historical
  Embench cases and checked selected core-gap values.
- **Limits:** analysis only; no new simulation, build, CTest or site build.
  Broad current-source core-gap and compiled performance remains unmeasured.

### Rewrite RV32I experiment results around final clock periods

- **Changed:** replaced §4.7 with final current-source clock certificates,
  current 35-case branch cycles and ticks, seven verified program-20 memory
  pairs, and dated broad fixed, core-gap and compiled results. Added the
  completed prefix and logic panels to the coverage inventory.
- **Why:** the old page mixed development chronology with results and could
  be read as applying historical measurements to current 166/80/100/102-tick
  clocks.
- **Checked:** rechecked ten safe-clock certificates and 14 current program-20
  runs; matched all new and transcribed result tables against artifacts.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed; retained inbound result-page anchors.
- **Limits:** broader FF, timed-memory and compiled campaigns were not rerun.
  The site build used existing example rows; WebAssembly packaging and browser
  tests were not run.

### Run current-source long branch structural comparisons

- **Changed:** completed all 35 long branch-pattern cases with oracle,
  expected-result and structural-reference checks; recorded the new result
  in §4.7 and a separate topic record.
- **Why:** the September 24 long branch supplements had 35 behavioral checks
  but no structural comparisons, and current-source evidence was needed.
- **Checked:** five seven-case campaigns and a one-case pilot completed at
  certified 166/80/100 tick periods; artifact audit verified 35 structural
  flags, 105 phase records, signature equality and `COMPLETE` markers.
  The serial native target build found the experiment binary up to date.
  `RV32IBenchmarkDocumentationTest`, strict MkDocs build and `git diff --check`
  passed.
- **Limits:** one measurement repetition with no host warmup; the broader
  core-gap, compiled and timed-memory campaigns were not rerun. The site build
  used existing example rows; WebAssembly packaging and browser tests were
  not run.

### Assess missing RV32I structural comparisons

- **Changed:** recorded measured-cost estimates for the 35 long branch cases
  and two full CRC32 cases, with the full Embench budget limits and a
  practical structural-check scope.
- **Why:** a completed behavioral/oracle result does not indicate whether
  adding a structural comparison is affordable on this host.
- **Checked:** compared historical structural run times, cycle/event counts,
  case summaries and campaign options; checked record links and whitespace.
- **Limits:** extrapolations use an older source/host. No new simulation,
  build, CTest or site build was run.

### Audit all recorded RV32I experiment outcomes

- **Changed:** recorded complete and partial campaign inventory, 19 × seven
  full Embench cycle results, compiled/fixed/core-gap evidence and explicit
  rerun decisions; updated living status to distinguish historical snapshots
  from current performance claims.
- **Why:** the published results page summarizes only selected completed
  campaigns, and new delay and memory-edge behavior changes the basis for
  present-day performance conclusions.
- **Checked:** compared `COMPLETE` markers, 1,179 case summaries,
  structural flags, periods and provenance; machine-checked all 133
  transcribed Embench values, record link targets and whitespace.
- **Limits:** artifact/documentation audit only. No experiment, CTest, source
  build, WebAssembly or site build was run; the broad current-source reruns
  remain open.

### Separate RV32I experiment setups from results

- **Changed:** moved dated campaign outcomes and clock-certificate values
  into §4.7; kept §4.2 focused on setup, test methods, artifact contracts
  and links to those results. Labeled the older 124/66/88 periods as an
  intermediate snapshot.
- **Why:** readers need a clear boundary between how experiments are run
  and what the completed runs established.
- **Checked:** the RV32I benchmark documentation CTest and strict MkDocs
  build passed; rendered result anchors and `git diff --check` passed.
- **Limits:** this is documentation work; no broad performance campaign or
  current-checkout Embench run, WebAssembly package or browser check was
  performed. The site build used the existing generated example rows.

### Update RV32I benchmark and verification chapter

- **Changed:** completed §4.2's campaign, recovery and external-runner test
  inventory, timed-memory handoff, certificate method and handshake-state
  regression; linked its compiled workloads to the results page.
- **Why:** let readers distinguish registered test passes, structural
  comparisons and full behavioral workloads while using current timing rules.
- **Checked:** six focused CTests passed serially; the documentation CTest
  passed again after the final wording edit; ten current safe-clock reports
  matched the table; strict MkDocs build, chapter links and `git diff --check`
  passed using the existing generated example bundle.
- **Limits:** no new performance campaign, full-suite rerun, WebAssembly
  package or browser checks. The performance README remains open follow-up
  work; the §4.7 clock claim was corrected in the entry above.

### Audit RV32I chapter 4.2 against experiments and benchmark tests

- **Changed:** recorded which chapter 4.2 contracts remain current and which
  harness tests, timed-memory validation details and completed compiled
  campaign links the public pages still need.
- **Why:** answer whether the chapter reflects the available experiment
  evidence before editing the public pages.
- **Checked:** compared chapter 4.2 and §4.7 with test registration, test
  source, campaign tools, completion artifacts and the delay record; checked
  the 13 benchmark-label CTest registrations without executing them.
- **Limits:** no public page, code, experiment or site build changed or ran.

## 2026-10-13

### Record RV32I test and experiment evidence for documentation planning

- **Changed:** completed the RV32I test inventory and dated campaign evidence
  record; opened a separate documentation-update record with a proposed page
  sequence, evidence boundaries and verification steps.
- **Why:** separate current registered checks and their recorded pass result
  from historical structural, reduced compiled and full behavioral campaigns
  before revising the public results.
- **Checked:** inspected CTest registration, test source, campaign completion
  and per-case metadata; ran current safe-clock certificates. The tracked
  clean-build record reports 451/451 native tests at `448e4e3`.
- **Limits:** no new full suite, performance campaign, site build or browser
  validation for this review; no public RV32I experiment page was edited.

### Align component references with current four-state behavior

- **Changed:** corrected public Fetch, Decode, memory-requester and pipeline-
  register behavior descriptions for unknown values; named the existing
  advanced Fetch, Memory and WB-bypass parts; updated the internal
  single-cycle stage reference to name `IncrementBy4`.
- **Why:** several component pages still described older binary fallbacks or
  conflated standalone requester state with the complete core's binary model.
- **Verified:** 11 selected documentation CTests passed serially; strict
  MkDocs build passed using the existing generated example rows;
  `git diff --check` passed.
- **Limits:** documentation only; the full native suite, WebAssembly package
  and browser checks were not rerun. RV32I experiment and test-case
  inconsistencies and behavioral delay computation descriptions were left out.

### Sort maintained internal docs into guides and reference

- **Changed:** moved three procedural or explanatory pages to `docs/guides/`
  and eight contract and architecture pages to `docs/reference/`. Added folder
  indexes, updated the placement rule in `AGENTS.md`, and repaired navigation
  links and the exact-path documentation test.
- **Why:** make a document's purpose clear from its location and leave the root
  of `docs/` for the index, living status and changelog.
- **Verified:** 1,024 source Markdown links resolved; the RV32I reference
  documentation test passed through CTest (1/1); `git diff --check` passed.
- **Limits:** generated simulator directory links are checked by the site
  build, which was not rerun. The move does not resolve stale claims found by
  the ongoing documentation audit.

### Delay model explained in one document

- **Changed:** added `docs/delay-model-explained.md`, a guided explanation of
  the delay work from the problem to the results (engine, delay tables, gate
  lists, X values, the safe clock, timed memories, experiments, limits), and
  linked it from `docs/README.md`.
- **Checked:** numbers copied from `docs/circuit-timing-analyzer.md`, the open
  delay record and the source; quoted commands and test names checked against
  the tools' options and the CTest list. No code changed; nothing rerun.
- **Limits:** it repeats the published-result gaps (advanced core at 100,
  timed memories at 102, not rerun).

### Advanced core clock covers the compact core; handshake states checked

- **Changed:** the certified clock includes the compact core's setup needs at
  its memory pins for every memory (advanced core with flip-flop memory: 98
  to 100). New validation option `--check-handshake-states`.
- **Why:** 98 was proven for the gates but only observed for the compact
  form; the handshake state lists had no check against simulation.
- **Verified:** every port state in program 20 is in its list for all five
  memories; nine clock and experiment tests pass.
- **Limits:** full suite and site build not rerun; published results at
  period 98 not rerun.

### Clock certified for cores with SRAM, DRAM, ROM and NOR-flash memory

- **Changed:** the safe-clock analysis enumerates the handshake states of a
  timed memory endpoint with the core's requester and settles each case;
  `--period auto` works for those memories (102 for the advanced core) and
  includes what the compact core's table needs at its memory pins.
- **Why:** the analysis stopped at a dependency loop between "request ready"
  and "response ready", so those systems needed a hand-picked period.
- **Verified:** gates and compact form agree at the automatic period for all
  four memories with 0 setup violations; 13 timing and experiment tests pass.
- **Limits:** full suite and site build not rerun; flip-flop memory unchanged
  at 98; no binary-startup certificate for timed memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Organize existing internal documentation

- **Changed:** moved 14 superseded guides, completed reports and older plans
  from the root of `docs/` to `docs/archive/`; added an index covering all 61
  archived documents. Reworked `docs/README.md` around the current developer
  references and repaired affected links in internal notes, the root README,
  agent instructions and a public site reference.
- **Why:** make the current guidance easy to find and keep historical claims
  separate from maintained contracts and procedures.
- **Verified:** the strict MkDocs build passed; 1,010 local Markdown document
  links resolved; the archive index covered every archived Markdown file;
  `git diff --check` passed. No C++ tests were run for this file relocation.
- **Limits:** archived content retains the claims and measurements of its
  original code state. This organization does not revalidate those claims.

### Compact parts follow their gates after X/Z; memories answer at clock edges

- **Changed:** compact parts leave X where their gates do; the self-timed
  memory sequencer shows its answer only at a clock edge; the timing analysis
  marks memory endpoints as keeping to the clock; delay tables regenerated;
  experiments report `setup_violations`; `--period auto` refuses non-FF
  memories; the `RV32IBinaryClockCLITest` limit went from 180 s to 300 s.
- **Why:** gate-level and compact forms disagreed after unknown inputs, and
  SRAM and DRAM systems lost responses or hung at some clock periods.
- **Verified:** native suite 450 of 451 in one serial run, the remaining test
  passing alone in 134 s; `DelayRuleTest` 0 rule breaks over 120,081
  situations; site build; 36 of 36 web checks; gate-level equals compact in 14
  system runs.
- **Limits:** published results for sequenced memories are not rerun; no clock
  is certified for cores with those memories. See the
  [record](records/2026-10-12-behavioral-delays-and-memory-clock-edge.md).

### Establish documentation recording rules

- **Changed:** made `AGENTS.md` the single agent rule set and made `CLAUDE.md`
  point to it. Added a living status, changelog and dated topic-record template.
  Moved the historical three-year roadmap out of the agent instructions and
  updated the internal documentation index.
- **Why:** keep current operating rules separate from historical plans and
  record substantive work without requiring a separate documentation request.
- **Verified:** `git diff --check` passed; 54 relative Markdown links in the
  edited and new documents resolved; the archived roadmap matched the former
  agent-file text verbatim; the dated record header and status listing agreed.
  No C++ tests or site build were run because source and `site-docs/` were not
  changed.
- **Limits:** this change establishes the workflow and an initial status. It
  does not revalidate every existing component claim or reclassify every older
  root-level internal document.
