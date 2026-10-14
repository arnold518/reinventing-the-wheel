# Changelog

Record meaningful completed changes here, newest first. Each entry states
what changed and why, what was verified, and any limits or deliberate
omissions. Entries begin with adoption of this recording workflow; older
history remains in Git and the archived reports.

## 2026-10-14

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
