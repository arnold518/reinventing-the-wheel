# RV32I test and experiment documentation update

Started: 2026-10-13. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

Chapter 4.2 documents experiment setup and verification contracts. The final
§4.7 reports 497 completed current-source panel case/target rows at certified
periods, compares cores and modes with a matched-panel chart, and separates
the conditional full Embench clock-only projection from measured results.
The former public clock-development narrative is preserved in the internal
archive. The performance runner's clock-contract README is current. Further
Embench execution would be a new measurement task, not unfinished prose.
This record links the page work to the completed
[evidence review](2026-10-13-rv32i-test-experiment-review.md); the separate
[documentation audit](2026-10-13-documentation-audit.md) tracks other pages.

## 2026-10-13 — Scope and current state

- The earlier [documentation audit](2026-10-13-documentation-audit.md) records the completed component-reference corrections and the still-open RV32I experiment and test-case findings.
- Current work is to inventory CTest scenarios, semantic and integration checks, structural/behavioral comparisons, timing certificates, campaign artifacts, and external ELF evidence before proposing page edits.
- The user requested a plan first. Public documentation changes remain proposals until that plan is discussed. Existing artifact results must be reported with their code state and completion evidence; a registered test or built ELF is not by itself a passing result.

## 2026-10-13 — Proposed page organization after evidence review

1. Keep the existing §4.2 structure. Add a concise test-kind and evidence matrix to `site-docs/rv32i/4.2/verification/index.md`, linking component contracts, ISA/fixed programs, core/lockstep, finite fidelity pairs, clock safety, campaign harness and real external workloads to their detailed pages. Identify the unit behind each count: CTest registration, differential pair, fixed-program checkpoint, benchmark case or campaign subprocess.
2. Recheck the detailed verification pages against `src/tests/`, `tests/checks/`, `tests/CMakeLists.txt` and the current runner. Explain that the structural reference in the 46-pair differential test is hybrid; the profile sweep does not enumerate all combinations; `FullEmbenchCampaignTest` and the default middle-suite CTest exercise orchestration with tiny stand-ins. Keep the already-correct 22-program, 15-family and 650-checkpoint descriptions.
3. Add a dated results/provenance table to `site-docs/rv32i/experiments.md` for the nine-case reduced-work compiled campaign and 133-case full Embench behavioral campaign, alongside the existing September 23–24 campaigns. Link it from `site-docs/rv32i/4.2/compiled/index.md` and `middle-equivalence.md` without turning a source catalogue into a pass list. State the source snapshot, workload scale, configurations, clock, verifier/oracle checks, structural-check flag and limits. Remove or date the stale unqualified 124/66/88 period claim.
4. Repair the clock-method sections in `tools/rv32i-performance/README.md` against the current analyzer and code. Distinguish current certificates (FF binary-startup 166/80/100; advanced unrestricted four-state timed memories 102) from historical performance runs; remove the retired period formula, 212-tick current claim and unsupported timed-memory statement. Review public timing and memory-wait prose for the new one-edge-later memory rule without changing historical result tables into unmeasured estimates.
5. Keep current-checkout performance claims pending a staged rerun. First run a small fixed/ELF and timed-memory smoke set with oracle, independent expected state, setup violations, and structural reference as applicable; then the fixed 22×7 and selected generated workloads; then the nine reduced compiled cases if resources permit. Run the 133-case full behavioral suite only as a separately budgeted refresh. Preserve source/ELF fingerprints, fidelity manifests, clock certificates, per-case summaries and `COMPLETE` markers, and report modeled ticks separately from host time. Avoid implying the historical campaigns were rerun by merely refreshing the prose.

## 2026-10-13 — Verification planned for the page edit

- Review exact page claims against current source and tracked evidence; run the focused RV32I documentation and benchmark CTests only where the edit changes a documented contract or generated examples.
- Run the strict MkDocs build for Markdown-only changes; use `tools/build-pages.sh` and web checks if the generated example rows or browser/WebAssembly content change. Check links and `git diff --check`.
- Record checks actually run and their limits here, then update `docs/STATUS.md` and `docs/CHANGELOG.md`. Until that edit is done, the public pages remain historical and this record remains open.

## 2026-10-14 — Chapter 4.2 currentness check

- Compared all chapter 4.2 overview, architecture, execution, timing, campaigns, verification and compiled-workload overview pages with `tests/CMakeLists.txt`, the relevant tests and campaign tools, the completed artifact inventory, and the open delay record. `ctest --test-dir build-cleanup -N -L benchmark` lists 13 registered benchmark-label tests. This was an inspection, not a new test run or site build.
- Accurate and worth preserving: 22 fixed + 15 generated catalogue entries; separation of workload, case, target and run; oracle versus independent expectations; hybrid structural-reference fidelity; 46 finite differential pairs; 650 fixed-program instruction attempts; distinction between cycle traces and aggregate reference comparisons; clock-certificate assumptions; campaign provenance and completion rules. The existing §4.2 structure is useful and does not need rewriting from scratch.
- The `index.md` description of verification as covering "Every benchmark validation suite" overstates the detail pages. `verification/infrastructure.md` omits the registered `FullEmbenchCampaignTest`, `BackgroundCampaignResumeTest` and `RV32IExternalBenchmarkRunnerSmoke`. The first two check a frozen campaign using smoke ELFs and recovery bookkeeping, not the full Embench or middle structural campaigns. The focused CTest command in `verification/index.md` is intentionally smaller than the entire benchmark-label set; make that explicit when updating the inventory.
- `verification/infrastructure.md` describes the earlier SRAM wait-path checks but not the current `RV32IExperimentCLITest` structural handshake-state checks for FF, SRAM and DRAM at `--period auto`, with zero setup violations. `verification/timing.md` likewise omits the new `--check-handshake-states` validation and the certified advanced-core timed-memory scope. The methodology in `timing.md` itself correctly avoids hard-coded historical clock periods.
- `compiled/middle-equivalence.md` gives expected outputs and how to run nine pairs, but does not report the completed 9/9 historical campaign. `compiled/index.md` rightly warns that a source catalogue is not a pass list, yet has no direct result link for the completed 133/133 full behavioral campaign. The separate `site-docs/rv32i/experiments.md` includes September 23–24 fixed/generated results but not either compiled campaign, and its unqualified 124/66/88 "subsequent" period claim is older than current 166/80/100 FF and 102 advanced timed-memory certificates.
- Correction to the 2026-10-13 proposal: the hybrid reference and finite-pair limits already appear clearly in `execution.md` and `verification/equivalence.md`; retain them instead of adding duplicate explanations. Focus page edits on missing harness tests, current timed-memory validation scope, links and dated outcomes. Existing completed pilots and incomplete directories under `build/experiments/` should remain distinct from the selected complete campaigns; a `COMPLETE` marker does not make an old source snapshot current.

## 2026-10-14 — Chapter 4.2 update started

- User authorized updating chapter 4.2. The edit will add a dated evidence summary to its campaign reference, complete the benchmark infrastructure inventory, and describe current clock and timed-memory checks while preserving the existing workload and fidelity definitions.
- The separate §4.7 historical-results page and `tools/rv32i-performance/README.md` remain open follow-up topics. Their stale claims must not be copied into chapter 4.2 as current measurements.

## 2026-10-14 — Public chapter edits in progress

- Updated the §4.2 index, execution/timing/campaign references, infrastructure and timing verification pages, and compiled/middle overviews. The chapter now distinguishes the 175-case fixed/legacy-loop campaign, 199-case core-gap expansion, 9-case reduced compiled structural campaign, and 133-case full behavioral campaign by date, scope and clock basis.
- Added the missing benchmark harness/recovery/compatibility test descriptions and current timed-memory handshake-state regression. Preserved existing workload pages, generated example rows and the structural-reference caveats already correct in §4.2.
- Validation remains to be run. No RV32I code, §4.7 result page or performance README was edited for this scoped chapter update.

The completed campaign artifact provenance used for the new table names Git
head `c003ed7d` with dirty worktrees and these distinct source SHA-256 values:

| Artifact | Source SHA-256 |
| --- | --- |
| `20260923-binary-safe-22/` | `e2483a9704ba04da9b67296e1bdeb26b200127cbdd1ee1b8e00e2ad690c1c1be` |
| `20260924-core-gap-rerun/` | `018bfee239849696bb0064bb4084aa711df188c847b6c51a204a28e4231cfc4c` |
| `20260928-middle-equivalence-background/` | `ed91061db501df90bfcd77c28dbc876acbe52d954e806afafca0574e1240a669` |
| `20260927-full-embench/` | `3d8092f2a130d85b0109b127eb65a9664f045b580ef9146f6b2cab6111c82d43` |

These hashes identify the archived runs; all four predate `448e4e3` and the
later delay/memory-edge changes. The September 24 supplemental cases have
their own provenance files and are included in the 199-case total documented
in §4.7.

## 2026-10-14 — Verification and remaining work

- This replaces the earlier "Validation remains to be run" note for the chapter edit. Six focused CTests passed serially in 113.43 seconds: `FullEmbenchCampaignTest`, `BackgroundCampaignResumeTest`, `RV32IExperimentCLITest`, `RV32IBenchmarkDocumentationTest`, `RV32IExternalBenchmarkRunnerSmoke` and `RV32IReferenceDocumentationTest`. After the final workload-test wording and clock table edits, `RV32IBenchmarkDocumentationTest` passed again (1/1).
- Ran `build-cleanup/rv32i_experiment --safe-clock` on the current binary for all three FF cores in both binary-startup and unrestricted-four-state scopes: 166/80/100 ticks, all `safe-under-contracts`. Advanced SRAM, DRAM, ROM and NOR-flash each returned 102 ticks under unrestricted-four-state analysis, also `safe-under-contracts`. These are certificates, not new program-performance results.
- A final strict MkDocs build passed in 44.39 seconds using the existing `build-wasm/pages/simulator/example-rows.json` bundle. The generated HTML contains the new campaign, clock and state-regression anchors. The chapter documentation test checked relative page links; `git diff --check` passed; all 72 chapter Markdown files were checked for trailing spaces/tabs.
- No code, generated example rows, new performance campaign, full native-suite rerun, WebAssembly package or browser checks were part of this chapter edit. The recorded clean build at `448e4e3` had already passed 451/451 native tests. The separate §4.7 page still has an older 124/66/88 period statement and lacks the compiled-campaign outcome table; the performance README has its own stale timing prose. This record stays open for those follow-ups.
- After a final provenance wording correction in `campaigns.md`, the strict MkDocs build passed again in 36.40 seconds. This later run is the final site-build check for the chapter edit; the earlier 44.39-second run also passed.

## 2026-10-14 — Chapter boundary correction

- The user clarified that §4.2 is for experiment setups and §4.7 for
  results. This replaces the earlier decision in “Chapter 4.2 update
  started” to put dated evidence in the campaign reference, and the
  “Verification and remaining work” note that §4.7 was still untouched.
- Removed completed campaign counts, measured core-gap observations,
  current clock values and the whole-suite pass result from §4.2. Its
  campaign, timing, compiled and verification pages retain setup, expected
  checks, supported scope and direct links to §4.7. The missing registered
  test descriptions and timed-memory validation method remain in §4.2.
- Added §4.7 coverage for the 175 fixed/loop, 199 core-gap, 133 full Embench
  behavioral, 9 reduced-work structural, and 7 program-20 campaigns, with
  their dates, fidelity checks and historical periods. The source snapshot
  is `c003ed7d` with a dirty worktree and distinct source fingerprints;
  `20260924-core-gap/` is incomplete and excluded. Added the current
  166/80/100 FF and advanced timed-memory 102-tick certificates as clock
  results, and labeled 124/66/88 an intermediate historical snapshot.
- This is a documentation placement and evidence correction. No performance
  campaign or source-code change was made. Verification for the corrected
  pages is recorded in the next dated section.

## 2026-10-14 — Verification of the corrected placement

- `RV32IBenchmarkDocumentationTest` passed (1/1) after the move. The strict
  MkDocs build passed with the existing generated example-row bundle; its
  rendered §4.7 contains the campaign and current-certificate anchors.
  `git diff --check` passed.
- Rechecked the October 3 artifact directly: `COMPLETE` exists, its summary
  has seven completed structural-reference cases with periods 92/58/78,
  and its provenance records source SHA-256
  `0d738d38f435aead863edf45b73dbab21f4966d25b9a72a9a8c8ca0e34834726`.
- This edit did not rerun the broad native suite, any performance campaign,
  the WebAssembly package or browser checks. The strict site build used
  existing example rows. The other campaign data and ten current safe-clock
  results were validated earlier in this record, not newly recomputed here.

## 2026-10-14 — Long branch result added to §4.7

- The separate [current-source run record](2026-10-14-rv32i-branch64-structural-rerun.md) documents 35/35 completed structural, oracle and expected-result comparisons. §4.7 now reports this dated result and its current cycles, while keeping the September 24 199-case result and its 164 structural checks unchanged as historical evidence.
- The broader FF, compiled and timed-memory result refreshes remain open. Site verification for this new page edit is recorded with the run record.

## 2026-10-14 — Full §4.7 rewrite started

- The user asked for a complete results-page rewrite using the final clock counts. The page will lead with the current certified FF periods (166/80/100 ticks) and advanced timed-memory period (102 ticks), then give the 35 current-source branch results with their measured cycles and modeled ticks. Broader completed campaigns remain dated results at their recorded periods; changing their period in prose would not make them current-source measurements.
- Rechecked the current experiment binary: six FF certificates (binary-startup and unrestricted four-state for each core) returned 166/80/100, and four advanced timed-memory unrestricted four-state certificates returned 102, all `safe-under-contracts`. This refreshes clock evidence only; it does not rerun the broad workloads.
- Preserve inbound result-page anchors used by the §4.2 pages and dated internal records. Verify transcribed tables against `build/experiments/`, run the focused documentation test and strict MkDocs build, and record their actual outcomes before closing this edit.

## 2026-10-14 — Current-source program 20 at the final periods

- Ran program 20 directly on `build-cleanup/rv32i_experiment` at `--period auto`, with `--max-cycles 10000`, `--max-events 100000000` and a 512 MiB allocation limit. The seven supported core/memory combinations were run in both behavioral and reference fidelity, 14 completed verified runs. Each pair matched cycles, retirement count (50), workload and final-state fingerprints, and reported zero setup violations. Advanced reference runs also used `--check-handshake-states`.
- Results at default not-taken policy: FF single 51 cycles × 166 = 8,466 ticks; FF baseline 138 × 80 = 11,040; FF advanced 85 × 100 = 8,500; advanced SRAM 400 × 102 = 40,800; DRAM 583 × 102 = 59,466; ROM 400 × 102 = 40,800; NOR-flash 334 × 102 = 34,068. The advanced reference runs checked 170/800/1166/800/668 handshake states for FF/SRAM/DRAM/ROM/NOR-flash respectively.
- Raw JSON and command manifest are in `build/experiments/20261014-program20-final-clocks/`. The manifest records source SHA-256 `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2`, binary SHA-256 `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1` and Git head `448e4e3`. This is a focused validation run, not a broad timed-memory campaign.

## 2026-10-14 — Full §4.7 rewrite completed and checked

- Replaced the previous chronology-heavy page with final current-source clock and result sections, followed by a clear inventory of dated broad campaigns. The current branch table reports cycles and modeled ticks at 166/80/100; the new seven-row program-20 table reports behavioral/reference pairs at 166/80/100/102. Historical fixed/loop, core-gap, full Embench and reduced compiled results retain their original periods. Added the previously omitted 175-case prefix and 175-case logic panels to the completed coverage table.
- Machine-checked 35/35 current branch cells, seven current program-20 rows, seven latest logic fixed-program totals, 21 logic loop cells, 133/133 full Embench cycle cells, nine/nine reduced compiled cells, and the selected core-gap and alias values against local artifacts. Preserved the five inbound §4.7 heading anchors used by current §4.2 pages and dated internal records.
- `RV32IBenchmarkDocumentationTest` passed (1/1). A strict MkDocs build passed using existing generated example rows; the rendered page contains all five retained anchors. `git diff --check` passed. WebAssembly packaging, browser tests, the full native suite and broad performance campaigns were not rerun for this page edit.
- The performance README and broader current-source performance refreshes remain open; this documentation record stays open for that separate follow-up.

## 2026-10-14 — Final-only results page plan and cost check

- The user narrowed the §4.7 requirement: the public page should contain the
  most recent measured ticks across all selected RV32I experiment tests, compare
  cores and settings, and contain no older result records. This replaces the
  earlier decision in “Full §4.7 rewrite started” to keep older campaign tables
  on the current results page. Preserve old artifacts and dated internal
  records as historical evidence; do not relabel their cycles with new periods.
- The current source/binary fingerprints in the 35-case branch artifacts and
  program-20 direct-run manifest are
  `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2`
  and `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1`.
  Those 35 checked branch outcomes and seven checked program-20 core/memory
  pairs may be reused if the final code/binary state and workload definitions
  remain the same. Program 20's memory pairs are direct runs, without a
  campaign `COMPLETE` marker; retain their raw manifest and verification
  status rather than presenting them as a completed campaign.
- Proposed FF matrix at one frozen source state and current binary-safe
  certificates (single/baseline/advanced = 166/80/100 ticks): 22 fixed
  programs × seven targets (154); all 15 generated families at a declared
  representative size/iteration/seed × seven (105); 23 core-gap configurations
  × seven (161); five 64-iteration branch patterns × seven (35, already current);
  three alias-capacity cases (3); 19 original full Embench workloads × seven
  (133, behavioral plus original verifier and ISA oracle); and three reduced
  Embench workloads × three cores (9, structural reference). This is 600
  selected case/target outcomes before any extra legacy-loop settings or
  timed-memory runs. Declare the exact input/size/window settings before
  execution; deduplicate only identical case definitions and preserve all
  meaningful settings. If retaining the earlier long-loop settings (dependent
  128, alternating 128, nested 32) separately from the representative
  generated panel, add 21 outcomes and label their different work explicitly.
- Include the two checked-in ELF fixtures described by §4.2 (the self-checking
  assembly image and project-owned compiled C image) on the three core kinds
  with the oracle, fixture verdict and structural reference. These are six
  further diagnostic outcomes, separate from Embench and from any seven-target
  predictor sweep. The 600-case matrix above becomes 606 with these fixtures;
  it does not count finite CTest registrations as performance cases. The
  expected-trap fixed programs and short ELF fixtures may have tick records,
  but exclude them from throughput summaries and geometric means.
- Rerun compatible advanced-core timed-memory workloads at the 102-tick
  certificate and record each memory technology, phase/guard settings,
  policy, cycles, ticks and wait observations. The existing native runner
  accepts `--memory`; the Python campaign driver does not expose it, so a
  repeatable provenance/completion wrapper is needed. ROM and NOR-flash
  cannot be treated as writable data RAM for every workload. Define a
  compatible input matrix and report unsupported pairings as such, not as
  missing results or performance failures. The seven current program-20
  core/memory pairs are a checked pilot, not broad memory evidence. A
  concrete first matrix is program 20 (instruction fetch/branching) and
  program 21 (read-heavy array sum) on all four timed technologies, plus
  program 22 (load/store copy) on SRAM and DRAM. Program 20 performs no
  data accesses, so its memory differences alone cannot characterize data
  latency; pilot program 21/22 before committing their compatibility claims.
- Use structural reference and oracle checks for bounded fixed/generated/
  core-gap/alias cases. Use the original verifier and oracle for full Embench;
  label its 133 results behavioral-only. Full structural Embench is not a
  practical routine requirement: the completed feasibility review projects
  about 549 serial hours for only three full algorithms × three targets.
  Keep the nine reduced-work complete-program structural comparisons separate
  from the 19 original full workloads.
- The previous full Embench artifact has 133 `COMPLETE` case directories and
  records 214.60 hours summed per-job wall time, with 312.67 hours between
  campaign start and finish (including interruptions/idle intervals). The
  nine-case middle suite records 8.50 summed job hours and 13.43 elapsed
  hours. These are historical host costs, not current-runtime guarantees.
  By comparison, the old fixed-program structural panel used 1.71 minutes
  of summed native execution, and the 161-case core-gap panel 14.92 minutes;
  the new 35 long-branch panel used about 6.5 minutes of summed native
  execution. Plan the Embench refresh as a multi-day serial background job,
  checkpoint it, and do not publish a partial matrix as a final ranking.
- For §4.7, generate tables from completed case summaries: every selected
  workload/input row and all seven target ticks (with cycle counts and periods
  available in the row/artifact), completeness/check flags, one fixed
  advanced policy for three-core comparisons, and a separate five-policy
  predictor comparison. Report per-workload winners, cycles versus modeled
  ticks, geometric-mean ratios only over the identical completed workload
  set, and counterexamples; do not pick the best policy separately per row.
  State that modeled ticks are simulator units, not physical time or an
  official Embench score. Keep §4.2 as the setup reference. Update live §4.2
  inbound anchors when §4.7 headings change, then check the generated tables,
  documentation test, strict MkDocs build and `git diff --check`.
- This turn inspected current source/tools and historical artifact costs; it
  did not start campaigns, rebuild code, run CTests or edit the public page.

## 2026-10-14 — Clarify varying propagation delay versus clock period

- The user pointed out that behavioral implementations do not have one fixed
  propagation delay for every operation. `BasicComponent::evaluateAt` selects
  delay arcs from the inputs that changed and, where present, the current
  control value; `publishTime` can use the gate-level delay twin's actual
  settling result for the present data. Memory devices also schedule their
  own response timing. This corrects any implication above that a 100- or
  102-tick period is the delay of each behavioral operation.
- The selected global clock period remains fixed for one experiment run and
  is a sufficient bound for the supported setup/hold/pulse contracts. The
  result schema computes simulated/modelled execution ticks as completed
  cycles multiplied by that period; it does not sum the changing internal
  propagation delays. Variable propagation and endpoint response timing can
  affect which edge captures a value and how many wait cycles the core takes.
  §4.7 must explain this distinction before comparing modeled ticks.

## 2026-10-14 — Why the four timed endpoints share a 102-tick certificate

- Re-ran `build-cleanup/rv32i_experiment --core advanced --memory NAME
  --safe-clock --allocation-limit-mib 512` serially for FF, SRAM, DRAM, ROM
  and NOR-flash. All five reported `safe-under-contracts`: FF 100 ticks and
  each timed technology 102 ticks. Each report's limiting constraint was
  `EXPERIMENT.IMEM.output.REQ_READY: full-cycle setup`; their reported
  175-node critical-path pin sequences were identical. The four timed
  endpoints have their own technology-specific proven/estimated delay tables
  and distinct state-case counts (SRAM/ROM/NOR-flash 36; DRAM 64), so equal
  periods are a result of the current maximum constraint, not identical
  component implementations or access times.
- `NativeSequence::step` schedules SRAM/ROM, DRAM and NOR-flash device work
  differently in ticks. Its response data, `DONE` and backpressure reach the
  requester at a clock edge; the clock analysis covers the external handshake
  states without treating the number of working cycles as a combinational
  path. Thus device duration appears mainly in how many CPU cycles wait.
  Current program-20 checks illustrate the distinction at 102 ticks:
  SRAM/ROM 400 cycles, DRAM 583, NOR-flash 334. Program 20 has no data
  accesses, so these differences concern instruction fetch in that test.
- The shared 102-tick value is scoped to this core, predictor/settings,
  endpoint configuration and timing contracts; changing them requires new
  certificates. This inspection and certificate rerun did not start a new
  workload campaign or edit §4.7.

## 2026-10-14 — Non-Embench full rerun launched

- Started a serial background run at 10:08 KST from the current `448e4e3`
  source in `build/experiments/20261014-nonembench-full/`. The clean build
  stage passed. The run first executes 450 CTests, excluding the one
  Embench-named harness test, then the 22 fixed-program, 15 generated-family
  and 23 core-gap presets across seven core/policy configurations (420
  experiment cases total). It does not execute full or reduced Embench ELFs.
- Every RV32I case requests the certified binary-startup clock, an oracle
  check, one measurement and a structural reference comparison. Generated
  and core-gap presets use eight iterations, size eight and seed one. The
  per-process limits are 300 seconds, 512 MiB address space, 100,000 cycles
  and 100 million events. Each preset gets a new output directory and its
  own `COMPLETE` marker; the controller stops on the first failure.
- `status.log` and per-stage logs under that run directory track progress.
  Historical wall times suggest about 30–45 minutes for the full run, but
  that is an estimate pending current-source progress. No result is claimed
  until CTest and all three campaign stages finish successfully.

## 2026-10-14 — Final report scope and Embench projection decision

- The user chose to keep full Embench as a projection rather than rerun its
  133 long behavioral cases. Compute projected modeled ticks as historical
  cycles times the current 166/80/100-tick FF periods, label every Embench
  value as conditional on unchanged cycles, and keep it separate from
  current-source measurements. Do not present the old 116/62/78-tick totals
  as current results or imply structural checks of the full suite.
- After the non-Embench campaigns finish, replace §4.7 with a comprehensive
  current-settings report: coverage and provenance, complete measured tick
  panels, three-core and predictor comparisons, memory-mode results, the
  labeled Embench projection, and limits. Keep §4.2 as setup guidance.
- The first two CTest timing analyses completed after 616.85 and 643.90
  seconds respectively, much slower than their recorded CTest costs. The
  original 30–45-minute ETA is no longer reliable; revise it using actual
  campaign progress. No public result-page edit has started yet.

## 2026-10-14 — Additional current-setting comparisons queued

- The three running presets use FF memory only. A second serial controller
  waits for their successful completion before running an advanced/two-bit
  memory panel: programs 19, 20 and 21 across FF, SRAM, DRAM, ROM and
  NOR-flash, plus write/copy program 22 on writable FF, SRAM and DRAM.
  Each of the 18 cases is an oracle-checked behavioral/structural-reference
  pair at its automatic certified period, with zero setup violations and
  structural handshake-state checking required. ROM and NOR-flash are not
  assigned the write/copy program.
- After that panel, the queued controller tests advanced/two-bit branch
  aliasing at 8, 16 and 32 predictor entries, then the checked-in assembly
  self-check and compiled C ELF across all seven core/policy targets, with
  structural comparisons. These are bounded current-source complements to
  the 420-case presets and the historical Embench projection. Every stage
  stops on failure and writes separate logs/artifacts under
  `build/experiments/20261014-nonembench-full/`.

## 2026-10-14 — Native-suite result and campaign resumption

- The first background CTest invocation finished 449/450 passes in 1,571.09
  wall seconds. `RegisterFileProfileMigrationTest` alone failed because
  Node's `spawnSync /usr/bin/python3` returned sandbox `EPERM`; its log shows
  no assertion failure. The exact test reran outside the sandbox and passed
  1/1 in 0.24 seconds. This is 450 tests accounted for across two
  invocations, not a single clean 450/450 suite pass.
- The controller stopped before beginning any RV32I performance case, as
  designed. A separate serial resume started the fixed-program preset at
  10:58 KST in the same run directory and will proceed to generated and
  core-gap. The timed-memory/capacity/compiled follow-up now waits on the
  resumed campaigns' own success marker. Original test logs and exit code
  remain intact; no completed case artifact was overwritten.

## 2026-10-14 — Broad FF-memory panels completed

- The resumed serial run completed fixed programs 154/154, generated
  families 105/105 and core-gap configurations 161/161: 420/420 selected
  cases. All 420 have `COMPLETE`-marked campaign directories, oracle-checked
  behavioral runs, matching structural-reference runs and matching
  measurement runs; all 1,260 phase records have zero setup violations.
  The three provenances agree on Git head `448e4e3`, source SHA-256
  `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2`
  and binary SHA-256
  `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcf08ab9dcd2857b0f1`.
  FF periods are 166/80/100 ticks across single/baseline/advanced.
- Each of the 154 fixed and 161 core-gap cycle counts matches the
  corresponding completed September panel, despite the updated source and
  clocks. This supports the clock-only Embench projection as a hypothesis,
  but does not verify any full Embench case. The current generated panel is
  a separate 15-family, eight-iteration/size-eight run.
- With advanced/two-bit fixed for the three-core comparison, modeled-tick
  winners are 11 advanced, 10 single and one baseline among the 22 short
  fixed programs; 14 advanced and one baseline among the 15 generated
  families; and 20 advanced and three baseline among the 23 core-gap
  configurations. The unweighted geometric-mean single/advanced and
  baseline/advanced tick ratios are 0.958 and 1.204 for fixed programs,
  1.345 and 1.696 for generated families, and 1.382 and 1.618 for core-gap.
  These are within-panel descriptive comparisons, not one aggregate score.
- The queued current-source memory, predictor-capacity and compiled-fixture
  follow-up began after the 420-case success marker. Its results remain
  pending; §4.7 still has not been edited for this final-only report.

## 2026-10-14 — Follow-up complete and final-only report verified

- This supersedes the pending follow-up statement in “Broad FF-memory panels
  completed.” The serial follow-up finished successfully: 18/18 compatible
  advanced/two-bit FF and timed-memory behavioral/reference pairs, three
  predictor-capacity comparisons (8, 16 and 32 entries), and both checked-in
  ELF fixtures on all seven targets (14/14). The follow-up and main campaign
  controller each ended with exit code zero. All these current-source cases
  passed their oracle or fixture verdict, structural comparison and clock
  setup checks; the memory reference checked observed handshake states.
  Program 22 was limited to writable FF, SRAM and DRAM.
- Program 21 retired 83 instructions and reached the same final state across
  FF, SRAM, DRAM, ROM and NOR-flash. Its respective cycles/ticks were
  110/11,000, 558/56,916, 807/82,314, 558/56,916 and 471/48,042. All four
  timed endpoints used the 102-tick period but took different working cycles.
  The capacity diagnostic recorded 67, 67 and five next-PC misses at 8, 16
  and 32 entries; cycles were 1,363, 1,363 and 1,239 at the 100-tick period.
- Replaced the public §4.7 page with complete current FF, long-branch,
  memory, capacity and fixture tables. Every current case/target row is
  rendered from a completed local summary or the focused program-20 JSON
  pairs. The total of 497 counts panel rows; some workload/target definitions
  recur across panels. The fixed-program
  three-core ratio excludes eight tiny trap cases and explicitly reports
  what including all 22 would change. The full Embench table instead projects
  all 133 historical cycle counts at the current FF periods; it is marked
  conditional and has no current-source or structural claim. The 19-row
  projected three-core comparison favors advanced/two-bit on 18 rows and
  baseline on `nettle-aes`; projected geometric-mean tick ratios are 1.425
  single/advanced and 1.488 baseline/advanced. Matching cycles in current
  fixed/core-gap/branch cases do not prove unchanged full Embench cycles.
- Adjusted §4.2 compiled and infrastructure links so setup pages identify
  the current fixture coverage, the conditional full-suite projection, and
  the absent current-source reduced-middle result. Old §4.7 result tables were
  removed; legacy anchors used by frozen dated records now point to a note
  about their historical evidence.
- `RV32IBenchmarkDocumentationTest` passed 1/1. A strict MkDocs build passed
  in 35.06 seconds using the existing
  `build-wasm/pages/simulator/example-rows.json` bundle, and `git diff
  --check` passed. The first MkDocs command used the system Python and failed
  because that interpreter lacks `mkdocs`; the installed
  `/home/arnold/venv/bin/python` succeeded. No generated example rows,
  WebAssembly package, browser tests or full Embench workloads were rerun
  for this documentation edit.
- A final completeness pass added the seven current-source default not-taken
  program-20 behavioral/reference pairs to the measured memory comparison;
  they are separate direct runs, with overlap against the broad FF panel.
  The count is now 497 panel rows rather than the earlier 490. Final
  documentation verification after this addition is recorded below.

## 2026-10-14 — Final documentation check after completeness pass

- `RV32IBenchmarkDocumentationTest` passed 1/1 after adding the focused
  program-20 table and adjusting the coverage count. The final strict
  MkDocs build passed in 36.68 seconds with the existing generated example
  bundle; `git diff --check` passed. The page contains complete current
  tables and the conditional full Embench projection. No current-source
  full or reduced Embench run, WebAssembly build or browser test was part of
  this check.

## 2026-10-14 — Results analysis and history removal started

- The user requested removal of historical analysis from public §4.7 and a
  dedicated performance-analysis section. The site navigation still includes
  `rv32i/4.7/timing-history.md`, a 387-line former clock-development
  narrative, and §4.2 links to it. Its old clock formulas and limitations
  are historical evidence, so move the page to the internal archive and
  remove its public navigation and setup-page links.
- Preserve the current-source result tables and the expressly conditional
  full Embench clock-only projection. Recompute comparisons from completed
  `20261014-nonembench-full` and five non-pilot long-branch summaries, hold
  advanced/two-bit fixed, and compare modeled ticks within matched panels.
  Give measured and projected evidence separate interpretations. The first
  read-only calculation found geometric-mean single/advanced and
  baseline/advanced ratios of 1.264/1.298 for 14 nontrivial fixed programs,
  1.345/1.696 for 15 generated families, 1.382/1.618 for 23 core-gap
  settings, and 1.366/2.062 for five long branch patterns.
- Add a compact graph only if it clarifies those within-panel comparisons;
  keep the exact values and sample sizes in text. Recheck source/artifact
  provenance and page links, then run the documentation check and strict
  MkDocs build after editing.

## 2026-10-14 — Final results analysis and archival verification

- Moved the former 387-line `site-docs/rv32i/4.7/timing-history.md` to
  `docs/archive/rv32i-timing-analysis-history.md`, indexed it, removed the
  public navigation entry and §4.2 links, and retained its dated claims as
  historical evidence. The strict site build has no historical clock page
  or navigation label. The conditional full Embench projection remains in
  §4.7 because the owner explicitly chose that reporting method; it is
  labeled separately from current-source measurements.
- §4.7 now has an explicit performance-analysis section and a regenerated
  SVG chart. Holding advanced/two-bit fixed, the matched current FF panels
  have unweighted geometric-mean `single/advanced` and
  `baseline/advanced` tick ratios of 1.264/1.298 for 14 nontrivial fixed
  programs, 1.345/1.696 for 15 generated families, 1.382/1.618 for 23
  core-gap settings, and 1.366/2.062 for five long branch patterns. The
  latter five all favor advanced/two-bit. Program 21's timed endpoint ticks
  relative to FF are 5.17× SRAM/ROM, 4.37× NOR-flash and 7.48× DRAM;
  32 predictor entries reduced the controlled aliasing case from 136,300
  to 123,900 ticks (9.1%) against 8/16 entries. These numbers are
  within-panel descriptions, not a cross-panel aggregate or physical speed.
- Recomputed the graph from completed `summary.json` files with
  `tools/rv32i-performance/plot-current-results.py`. It checks every row's
  completion, structural comparison, period and cycle×period relationship,
  plus the shared source and binary SHA-256 in all eight panel provenance
  files. The SVG was visually inspected using a PNG render; its XML parses.
  The current report's chart link resolves in the built site.
- `RV32IBenchmarkDocumentationTest` passed 1/1. The final strict MkDocs
  build passed using the existing generated example-row bundle;
  `git diff --check` passed. No new benchmark, full native-suite,
  WebAssembly or browser run was performed for this documentation change.
