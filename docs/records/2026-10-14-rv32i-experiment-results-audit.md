# RV32I experiment results and rerun audit

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

Thirty-nine complete artifact directories contain 1,179 case/target summaries
across repeated RV32I source snapshots; 1,009 include a structural reference
check. The broad performance campaigns are historical. Current source has
clock certificates and a full native test pass, but no refreshed broad
performance or full Embench campaign. To make current performance claims,
rerun FF panels and timed-memory workloads; rerun compiled panels only for
current compiled-workload claims. The dated old results need no rerun merely
to remain valid historical evidence.

## 2026-10-14 — Scope and sources

- Read `docs/STATUS.md`, `docs/README.md`, the open RV32I documentation and
  behavioral-delay records, §4.2 setup and §4.7 results, archived benchmark
  reports, and the current `build/experiments/` artifact tree.
- Treat a campaign's `COMPLETE` marker, case summaries, validation mode,
  period and source fingerprint as separate evidence. Repeated cases across
  source snapshots are not independent new workloads. An incomplete directory
  or a smoke fixture is not a full benchmark result.

## 2026-10-14 — Complete artifact inventory

`build/experiments/` contains 39 top-level directories with `COMPLETE`:
36 direct-summary campaigns and three directory/background campaigns. Their
summaries contain 1,179 completed case/target rows, including 1,009 with a
structural-reference check. These totals include repeated workloads across
development snapshots and must not be reported as 1,179 independent programs.
All 190 inspected provenance files name Git head `c003ed7d`; their dirty
worktrees yield 18 distinct source SHA-256 values. None is a campaign on
current source state `448e4e3`.

| Artifact cohort | Completed rows / structural checks | Scope and result |
| --- | ---: | --- |
| 2026-09-23 candidate-clock work: `core-pilot-final` (154), `correctness-clocks` (48), `pilot-v2` (42), four `capacity-*` sweeps (8), `reference-mode-smoke` (1) | 253 / 253 | Early fixed-program, predictor-capacity and reference smoke checks at candidate periods 458/320/394. The period formula did not certify safety; later certified runs supersede timing claims. |
| 2026-09-23 binary-safe work: `binary-safe-22` (154), three `binary-safe-*128`/`nested32` loops (21), `binary-safe-smoke` (7) | 182 / 182 | Oracle and structural agreement at certified 176/112/212 periods. The 175 non-smoke cases are the published fixed/loop campaign. |
| 2026-09-23 prefix/recovery work: `prefix-branch-recovery` (7), `prefix-final` (7), `rerun-programs22` (154), three `rerun-*` loops (21) | 189 / 189 | Full fixed/loop panel after carry/recovery/fetch changes at 124/66/88; additional program-20 checks overlap it. |
| 2026-09-24 logic work: `logic-programs22` (154) and three `logic-*` loops (21) | 175 / 175 | Structural/oracle checks after combinational simplification at 116/62/78; paired workloads retained the prior cycle counts. |
| 2026-09-24 core-gap: `core-gap-rerun` (161), five `branch64-*` (35), three `alias64-entries*` (3) | 199 / 164 | Dependency, scheduling, branches, aliasing, calls and memory kernels. The 35 long-pattern cases had oracle/expected-result checks without structural reference. |
| 2026-09-26 `current-smoke` | 21 / 21 | Three fixed programs across seven targets, bounded structural/oracle smoke at 116/62/78. |
| 2026-09-26 `current-embench-crc32-retry-22m` | 2 / 0 | Completed behavioral CRC32 on baseline and advanced only, at 62/78. It does not represent a full suite or all cores. |
| 2026-09-27 `full-embench` | 133 / 0 | Nineteen full compiled workloads across seven targets; original verifiers and instruction oracle passed on behavioral cores at 116/62/78. |
| 2026-09-28 `middle-sha-pipelines` (2) and `middle-equivalence-background` (9) | 11 / 11 | A two-core SHA pilot plus three reduced-work algorithms across three cores; complete-program structural-reference and oracle checks at 116/62/78. |
| 2026-10-02 `register-clock-refresh` | 7 / 7 | Program 20 across seven targets at 92/58/78. |
| 2026-10-03 `advanced-grouping-clock-final` | 7 / 7 | Final program-20 snapshot at 92/58/78; all seven summaries are completed and structurally checked. |

The incomplete `pilot-v1`, `core-gap`, initial/detached/4-hour CRC attempts,
`middle-sha-equivalence`, and `advanced-grouping-clock` have no top-level
`COMPLETE`. The last has seven summary rows, but still lacks campaign
completion. `20261009-delay-estimate/` contains Markdown analysis, not a
campaign or a failed run. These are excluded from the completed totals.

## 2026-10-14 — Clock and fixed/generated workload results

The first four rows below sum **programs 17–22**, each started separately.
They diagnose a small fixed workload panel; they are not a normalized
benchmark score. Each value is modeled ticks, using that snapshot's periods.

| Snapshot | Periods: single / baseline / advanced | Single | Baseline | Advanced two-bit | Evidence |
| --- | --- | ---: | ---: | ---: | --- |
| Candidate pilot, 2026-09-23 | 458 / 320 / 394 | 184,116 | 317,440 | 198,182 | Structural/oracle pass; candidate period, not safety certified |
| Binary-safe, 2026-09-23 | 176 / 112 / 212 | 70,752 | 111,104 | 106,636 | 175 published fixed/loop cases, all structural |
| Prefix/recovery, 2026-09-23 | 124 / 66 / 88 | 49,848 | 65,472 | 44,000 | 175 fixed/loop reruns, all structural |
| Logic simplification, 2026-09-24 | 116 / 62 / 78 | 46,632 | 61,504 | 39,000 | 175 fixed/loop reruns, all structural |
| Register/grouping work, 2026-10-02/03 | 92 / 58 / 78 | — | — | — | Program 20 only, seven cases at each snapshot |
| Current source `448e4e3` | FF 166 / 80 / 100 | — | — | — | Certified periods; no broad performance rerun |

The published [September 23 results](../../site-docs/rv32i/experiments.md#measured-result-binary-initialized-clocks)
also show the three longer loops across all seven targets. The archived
[logic comparison](../archive/rv32i-logic-timing-optimization.md#measured-comparison)
records the later 175-case panel: 22-program diagnostic totals of
75,400/94,550/66,846 ticks (single/baseline/advanced two-bit) and
dependent-loop totals of 74,704/127,844/51,012. It verified equal cycle
counts and final-state fingerprints against the prefix snapshot; its modeled
time reductions follow from changed periods. Program 20 on the final
2026-10-03 snapshot took 51 single, 138 baseline and 61 advanced/two-bit
cycles, at 92/58/78 ticks. That is a focused historical run.

The current analyzer's ten checked reports returned `safe-under-contracts`:
FF single/baseline/advanced = **166/80/100** for binary startup and
unrestricted four-state; advanced SRAM/DRAM/ROM/NOR-flash = **102** under
unrestricted four-state. These are sufficient modeled periods, not physical
frequencies or program-performance measurements. They do not validate
reusing old modeled ticks at the new clock.

## 2026-10-14 — Core-gap mechanisms and result limits

The 161-case main panel used 23 parameterized workloads × seven targets,
with oracle, independent expected-result and structural checks on every
case. The 35 longer branch-pattern cases checked the oracle and expected
result but did not run structurally; the three fixed-image predictor-capacity
cases did. The [archived report](../archive/rv32i-core-gap-benchmarks.md#completed-main-panel)
and [§4.7 tables](../../site-docs/rv32i/experiments.md#expanded-workload-results-2026-09-24)
hold the per-case modeled ticks. Key measured mechanisms:

- Dependency distance one: baseline **2,137 cycles**, advanced/two-bit
  **561**; at distance four/eight baseline falls to **625** while advanced
  stays at **561**. This isolates stall-only versus forwarding behavior.
- Equivalent load-use instruction sets: scheduling removes **64** advanced
  bubbles and changes two-bit execution from **292 to 228 cycles**.
- Predictor training on the tested branch-body region: two-bit **74 to 66
  cycles**, five to one next-PC misses, with the same 56 retired instructions.
- Fixed 64-iteration alias image: two-bit table sizes 8/16 each have
  **1,363 cycles and 67 misses**; 32 entries have **1,239 cycles and five
  misses**. All three were structurally checked at 78 ticks.
- Five 64-iteration branch-pattern runs cover the full 32-outcome sequence
  twice. The short eight-iteration main-panel burst case does not cover
  pattern transitions; the supplemental runs do.

These observations identify mechanisms on selected inputs. They do not
establish a universal predictor winner, cache behavior or current modeled
hardware speed.

## 2026-10-14 — Compiled workload results

The full Embench campaign has 19 original compiled workloads × seven
configurations, 133 completed behavioral/oracle/verifier cases and **zero
structural checks**. The cycle table below transcribes every case summary.
Advanced columns are not-taken (NT), taken (T), backward-taken (BT), one-bit
(1b) and two-bit (2b); single and baseline use not-taken. Periods were
116/62/78 ticks, so a historical modeled-time cell is its cycle count times
that core's period. These are not official normalized Embench scores.

| Workload | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| aha-mont64 | 11,425,502 | 16,739,060 | 14,019,466 | 12,357,832 | 12,564,698 | 12,380,704 | 12,312,016 |
| crc32 | 6,130,443 | 19,966,346 | 7,356,891 | 7,007,039 | 7,007,033 | 7,007,375 | 7,007,039 |
| cubic | 30,287,860 | 71,399,587 | 42,590,615 | 37,505,317 | 37,296,609 | 37,130,343 | 36,513,847 |
| edn | 75,135,316 | 191,060,158 | 111,910,206 | 92,733,864 | 89,051,016 | 83,631,098 | 83,518,500 |
| huffbench | 2,693,952 | 5,749,891 | 3,365,737 | 3,215,457 | 2,922,743 | 2,963,505 | 2,940,475 |
| matmult-int | 29,506,443 | 74,426,421 | 44,177,157 | 36,564,643 | 36,015,495 | 37,160,367 | 36,407,605 |
| minver | 12,751,619 | 29,064,911 | 17,987,531 | 16,165,139 | 15,762,493 | 15,306,379 | 15,227,437 |
| nbody | 27,246,935 | 65,541,011 | 38,868,841 | 33,659,173 | 32,939,523 | 33,132,073 | 32,826,229 |
| nettle-aes | 4,807,300 | 5,875,634 | 5,044,530 | 5,046,090 | 4,957,130 | 4,998,206 | 4,940,870 |
| nettle-sha256 | 4,099,001 | 5,219,708 | 4,206,627 | 4,177,117 | 4,158,071 | 4,168,539 | 4,160,961 |
| nsichneu | 2,238,725 | 6,562,009 | 3,379,608 | 4,175,512 | 3,377,174 | 3,406,714 | 3,354,964 |
| picojpeg | 5,241,766 | 11,818,952 | 6,129,158 | 5,591,750 | 5,768,474 | 5,537,090 | 5,519,994 |
| qrduino | 6,032,091 | 15,169,390 | 7,948,406 | 7,543,352 | 7,374,356 | 7,556,310 | 7,317,850 |
| sglib-combined | 2,975,542 | 6,906,791 | 3,828,677 | 4,063,561 | 3,808,805 | 3,698,971 | 3,637,847 |
| slre | 2,533,787 | 5,732,802 | 3,104,696 | 3,452,584 | 3,104,482 | 3,061,616 | 2,969,702 |
| st | 17,895,271 | 42,991,287 | 25,458,239 | 22,453,967 | 21,590,901 | 21,850,025 | 21,484,209 |
| statemate | 1,684,582 | 3,023,302 | 1,938,243 | 1,875,385 | 1,914,555 | 1,887,029 | 1,855,627 |
| ud | 6,771,211 | 14,687,130 | 8,966,161 | 9,051,945 | 8,658,491 | 8,868,513 | 8,531,311 |
| wikisort | 6,310,064 | 14,142,504 | 8,959,126 | 8,149,074 | 7,926,686 | 7,644,544 | 7,552,916 |

For the fixed single/baseline/advanced-two-bit comparison, advanced two-bit
has the lowest old modeled time on 16 workloads, baseline on two and single
on one. This is an inference from those 19 rows and the historical periods,
not a current or cross-workload normalized score. The 133 cases preserve
their own ELF hashes, manifests, run records and verdicts beneath
`build/experiments/20260927-full-embench/cases/`.

The reduced-work middle campaign gives a separate **9/9 full-program
structural-reference** result. It runs one invocation of each algorithm,
including startup and original verifier, and compares aggregate signatures
and final memory, not cycle-by-cycle internal signals.

| Reduced workload | Single cycles / ticks | Baseline cycles / ticks | Advanced two-bit cycles / ticks |
| --- | ---: | ---: | ---: |
| CRC32 | 35,907 / 4,165,212 | 116,894 / 7,247,428 | 41,061 / 3,202,758 |
| SHA-256 | 8,767 / 1,016,972 | 11,314 / 701,468 | 8,949 / 698,022 |
| SGLIB combined | 105,470 / 12,234,520 | 246,502 / 15,283,124 | 129,941 / 10,135,398 |

Earlier completed compiled pilots overlap the final campaigns: the
2026-09-26 CRC retry has baseline **20,141,448 cycles** and advanced/two-bit
**6,831,935** (behavioral only); the 2026-09-28 two-core SHA pilot matches
the baseline/advanced entries above. The 21-case September 26 smoke set
checks programs 1, 17 and 20 across seven targets; it is not an external
suite.

## 2026-10-14 — Earlier external and timed-memory evidence

- The archived [external validation report](../archive/rv32i-external-validation-report.md#result)
  records 39/39 ACT4 I-tier ELFs passing Sail and behavioral CircuitSim, plus
  one structural `I-nop-00` case; a full structural ACT4 sweep was never run.
  This predates the current core and is separate from the 19-workload Embench
  campaign.
- The archived [August CRC32 report](../archive/rv32i-external-benchmark-report.md#result)
  records one full CRC32 ELF passing its verifier on the then-current single
  and five-stage behavioral cores: 6,130,443 and 7,181,789 cycles,
  respectively, and a **1.494× modeled speedup** at historical periods
  189/108. It is not the later seven-target full Embench measurement or a
  current-core result. The companion
  [timing report](../archive/rv32i-timing-performance-report.md#hardware-results)
  gives programs 17–22, timing and host-cost measurements on the same old
  architecture.
- The open [delay/memory-edge record](2026-10-12-behavioral-delays-and-memory-clock-edge.md#2026-10-13--flip-flop-clock-includes-the-compact-core-state-lists-checked)
  reports structural program-20 handshake-state coverage for FF, SRAM, DRAM,
  ROM and NOR-flash at automatic periods, all verified with zero setup
  violations. Its earlier 14-run SRAM/DRAM/ROM/NOR cycle observations used
  fixed periods before the final 102-tick timed-memory certificate and are
  diagnostic history, not a current performance panel.
- The current clean rebuild passed **451/451 native CTests** at `448e4e3`.
  This covers registered bounded regressions; it does not execute the full
  Embench or middle structural campaigns. The full suite is not rerun by
  this audit.

## 2026-10-14 — Which results need rerunning

| Desired claim or use | Decision | Reason and minimum evidence |
| --- | --- | --- |
| Preserve the dated candidate, binary-safe, prefix, logic, core-gap, Embench and middle results as history | **No rerun** | Each completed artifact still describes its own identified source/ELF/clock. Keep its date, source fingerprint, completion and fidelity scope visible. A rerun on today's source would be a new result, not a replacement for that historical measurement. |
| Claim present-day performance or relative ranking for FF-memory cores | **Rerun required** | Every completed broad campaign predates `448e4e3`. Code since `c003ed7d` changed delay tables, some core internals, and clock analysis. Start with program 1/17/20 and an external self-check at current certified periods; then rerun the fixed 22 × seven and selected generated/core-gap panels with oracle, expected state and structural reference. Retain new provenance and `COMPLETE`. |
| Claim present-day cycle and modeled-time cost for SRAM, DRAM, ROM or NOR-flash systems | **Rerun required** | The memory sequencer now exposes completion on a clock edge, adding a cycle per access, and the advanced/timed-memory certificate is 102. Run each supported endpoint with read/write-appropriate workloads; record cycles, wait observations, oracle/final state, handshake coverage and setup violations. Prior cycle totals cannot be fixed by multiplying by a new period. |
| Reuse an advanced/FF result previously evaluated at period 98 as a certified current-time result | **Recheck required** | The current bound is 100. At unchanged cycles, modeled ticks would scale by 100/98, but a run is needed to verify that cycles and final state are unchanged. No completed `build/experiments/` summary in this audit uses period 98; the 98-period publication warning comes from the delay record/guides, not an identified complete campaign here. |
| Claim current complete-program structural agreement for compiled algorithms | **Rerun the nine-case middle panel** | The historical 9/9 is at an older source SHA. The CTest harness uses tiny stand-ins; it does not replace the real reduced-work ELFs. Preserve original verifiers, oracle, structural signatures and ELF hashes. |
| Claim current full Embench coverage or speed comparison | **Rerun the 133-case behavioral panel** | The old 133/133 has no structural checks and predates current source. This is a separate, expensive refresh; do not infer it from the bounded `FullEmbenchCampaignTest` or the middle panel. A representative single ELF can be run first. |
| Maintain current clock-certificate and native-regression claims at `448e4e3` | **No immediate rerun** | Ten safe-clock reports and a clean 451/451 CTest result are already recorded at this source state. Recompute after relevant source changes. They do not by themselves make old performance campaigns current. |
| Reproduce older candidate-clock, prefix-versus-ripple or prior-architecture speedup figures | **Only if studying that historical comparison** | These are intentionally different circuits and clock contracts. Running the new code cannot reproduce both sides of a past before/after experiment. |

The public [§4.7](../../site-docs/rv32i/experiments.md) currently summarizes
the binary-safe, core-gap, full Embench, middle and final program-20
campaigns, but omits the separately completed 175-case prefix and 175-case
logic panels, the earlier external/ACT4 evidence, and focused pilots. The
archived reports and this inventory retain their results. Any public
expansion should label every historical snapshot and avoid treating a
source-age or clock difference as a current speedup.

## 2026-10-14 — Verification and limits

- Read artifact `COMPLETE` markers, direct/nested summary arrays, completion
  statuses, structural-check flags, periods and provenance. The direct
  summaries account for 1,035 rows (1,000 structural); the three directory
  campaigns add 144 rows (nine structural), yielding the totals above.
- Checked the 133 full Embench and nine middle case summaries directly;
  their parent `campaign.json` status is `completed`. The 19 × seven cycle
  matrix above is transcribed from the full campaign case summaries.
- Inspected the source changes after `c003ed7d` and the current delay record.
  This was read-only artifact analysis. No experiment, CTest, build or site
  build was run for this audit; no ignored artifact was edited. Historical
  host times, modeled ticks and old clock bounds remain snapshot-specific.
- Checked the transcribed 19 × seven full Embench cycle matrix against all
  133 JSON summaries, every relative link target in this record, trailing
  whitespace and `git diff --check`; all passed.
