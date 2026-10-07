# § 4.7. Experiment Results

[Benchmark system](4.2/index.md) · [Measurement contract](4.2/execution.md) · [Campaigns](4.2/campaigns.md)

These are **historical measurements of identified circuit/build snapshots**, not automatically refreshed results for the current checkout. The documentation migration did not rerun these campaigns. Reproducing a result requires its workload image, core/predictor/memory configuration, fidelity manifest, clock certificate, source/binary fingerprints, and completion evidence.

Methods and workload definitions now live in [4.2](4.2/index.md). Earlier timing-analysis development is preserved in the [timing history](4.7/timing-history.md); its old bounds and unsupported cases must not be used as current certificates. Host speed, modeled hardware time, and verification coverage are different quantities.

## Expanded workload results, 2026-09-24

The inventory now has **22 fixed programs plus 15 generated workload families**
(three legacy loops and twelve new families). Parameter variations are cases,
not additional fixed programs. External ELF fixtures are separate.

The completed `core-gap` panel used `iterations=8`, `size=8`, `seed=1`, and all
seven core/predictor configurations. All **161 cases** passed oracle checks,
independent expected-result checks, and structural/behavioral comparisons.
Cross-core program/input bytes, final states and retired counts also matched.
Clocks were 116/62/78 ticks; all advanced predictors used 16 entries.

Selected results below are modeled ticks, smaller being better. Advanced uses
the same **two-bit policy** in every row, not the best predictor chosen after
seeing each result.

| Workload | Single-cycle | Baseline | Advanced / two-bit |
| --- | ---: | ---: | ---: |
| Independent ALU, one iteration | 10,324 | 7,936 | 7,254 |
| Independent ALU, eight iterations | 63,916 | 38,750 | 43,758 |
| Dependency distance 1 | 63,916 | 132,494 | 43,758 |
| Dependency distance 2 | 63,916 | 69,502 | 43,758 |
| Dependency distance 4 or 8 | 63,916 | 38,750 | 43,758 |
| Immediate load-use | 25,288 | 29,016 | 22,776 |
| Scheduled load-use | 25,288 | 25,048 | 17,784 |
| Calls/returns | 19,720 | 26,908 | 20,280 |
| Streaming copy | 60,204 | 71,672 | 47,814 |
| Pointer chasing | 45,356 | 75,640 | 42,822 |
| Byte scan | 50,924 | 77,128 | 41,574 |
| Insertion sort (two iterations) | 52,548 | 72,292 | 45,630 |
| CRC-32 (two iterations) | 95,468 | 135,098 | 81,042 |

Baseline wins on long independent arithmetic: its shorter clock outweighs the
remaining control overhead. Close dependencies reverse this result because
baseline waits instead of forwarding. Advanced takes 561 cycles at every tested
dependency distance; baseline takes 2,137 at distance 1 and 625 at distance 4/8.

The load schedules execute the same operations. Scheduling removes 64 advanced
load-use bubbles, reducing 292 cycles to 228. Calls/returns remain 2.84% slower
on advanced/two-bit than single-cycle in this case; no BTB or return-address
stack was added. A more advanced core is not necessarily fastest on every task.

For the taken-branch body region, 32 prefix iterations reduce two-bit execution
from 74 to 66 cycles and observed next-PC misses from five to one. Both regions
retire 56 instructions. The predictor is genuinely trained at the same PCs;
host-process warmup is not responsible for this change.

### Longer branch patterns

Eight iterations cover only part of the 32-outcome sequence (the burst case sees
only its first not-taken burst). Therefore five supplemental 64-iteration runs
cover each sequence twice, on all seven configurations. These **35 cases** use
the oracle-checked behavioral implementations, backed by the smaller structural
checks; they are not another full structural campaign.

| Pattern | Single | Baseline | Adv. not-taken | Adv. taken | Adv. backward-taken | Adv. one-bit | Adv. two-bit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Taken | 53,128 | 93,062 | 60,840 | 41,184 | 51,168 | 41,496 | 41,808 |
| Not-taken | 60,552 | 97,030 | 55,848 | 56,160 | 46,176 | 46,332 | 46,488 |
| Alternating | 56,840 | 95,046 | 58,344 | 48,672 | 48,672 | 53,664 | 48,984 |
| Bursts | 56,840 | 95,046 | 58,344 | 48,672 | 48,672 | 44,928 | 46,176 |
| Irregular | 56,608 | 94,922 | 58,500 | 48,204 | 48,828 | 48,828 | 48,828 |

Numbers are modeled ticks. Pattern names describe the forward data branch;
the mostly-taken backward loop branch is also measured. One-bit adapts faster
at burst transitions; two-bit fares better on this alternating sequence. The
irregular sequence is deterministic and short, not proof about arbitrary code.

### Predictor capacity and aliasing

With a fixed 64-iteration alias workload, two opposite-outcome branch PCs are
`0x10` and `0x50`, followed by loop branch `0x5C`. Only table size changes:

| Two-bit entries | Period | Cycles | Next-PC misses | Modeled ticks |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 78 | 1,363 | 67 | 106,314 |
| 16 | 78 | 1,363 | 67 | 106,314 |
| 32 | 78 | 1,239 | 5 | 96,642 |

The first two branches share an index at 8/16 entries but separate at 32. All
three configurations have structural/behavioral checks and independently
recomputed certificates. Another circuit path remains limiting here; equal
periods do not mean larger tables have no area/power cost.

### Evidence and limits

The main panel and supplements total **199 cases, 164 structural checks, and
1,159 recorded subprocess runs**, all successful. Each case has one host warmup
and three measurements. Native processes run serially with a 512 MiB allocation
cap; maximum reported RSS across these runs was 87,908 KiB (85.8 MiB). Builds
and documentation generation are outside that figure. The full repository test
suite was not run; four focused experiment CTest targets passed.

Completed artifacts are `build/experiments/20260924-core-gap-rerun/`,
`20260924-branch64-{pattern}/`, and `20260924-alias64-entries{8,16,32}/` under the
same experiments directory. They contain provenance, exact case arguments,
certificates, summaries, raw runs and `COMPLETE` markers. The original
`20260924-core-gap/` was interrupted by an environment change and is excluded.

These are diagnostic workloads, not a calibrated real-chip benchmark score.
All memory is zero-wait FF; no cache conclusions follow from these results.
The core circuits were frozen throughout this expansion.

## Measured result: binary-initialized clocks

These are preserved **pre-optimization** results. The subsequent carry-prefix,
parallel-recovery and fetch-control changes reduce the calculated sufficient
periods to **124 / 66 / 88 ticks** (single / baseline / advanced). Do not reuse
the older campaign's host timings or hardware-time totals for the changed
circuits; the campaign directories below identify the exact measured snapshot.

The 2026-09-23 campaigns use zero-wait FF instruction/data endpoints, the
structural reference profile for timing and cross-checks, and 16-entry advanced
predictors. All seven configurations have a successful binary-startup safety
certificate. The selected periods are 176 ticks for single-cycle, 112 for
baseline, and 212 for every advanced predictor policy. Each campaign used a
512 MiB address-space limit, one host warmup and three measured repetitions per
configuration. Every run starts with a cold predictor and a zero-filled memory
image into which its binary program is loaded.

The full shared-program campaign covered **22 programs × 7 configurations =
154 cases**. Each case passed an instruction oracle and full final-memory
check, and a separate structural reference run matched its compact run. There
were 154 warmups and 462 measured runs. Programs 1–16 mainly test correctness
and faults; the performance panel is programs 17–22. The following totals mean
running each of those six independent programs once; they are not a weighted
benchmark score.

| Core / predictor | Cycles across 17–22 | Modeled ticks across 17–22 |
| --- | ---: | ---: |
| Single-cycle | 402 | **70,752** |
| Baseline | 992 | 111,104 |
| Advanced / not-taken | 556 | 117,872 |
| Advanced / taken | 491 | 104,092 |
| Advanced / backward-taken | 491 | 104,092 |
| Advanced / one-bit | 497 | 105,364 |
| Advanced / two-bit | 503 | 106,636 |

Baseline has the lowest modeled time for program 17 (8,624 ticks); single-cycle
is lowest for each of programs 18–22. The earlier 154-case candidate-clock
pilot and this campaign have identical cycles, instruction counts, final state
fingerprints and simulator event counts for every matched case. The modeled
tick changes therefore follow from the chosen clock periods. The pilot's
candidate clocks were 458/320/394 ticks; they were not safety certificates.

Three generated loops exercise sustained dependencies and branches. The nested
case has 32 outer iterations and eight inner iterations per outer iteration;
the other two use 128 iterations. Each cell below is `cycles / modeled ticks`.

| Core / predictor | Dependent 128 | Alternating 128 | Nested 32 |
| --- | ---: | ---: | ---: |
| Single-cycle | 644 / **113,344** | 580 / **102,080** | 868 / **152,768** |
| Baseline | 2,062 / 230,944 | 1,870 / 209,440 | 2,379 / 266,448 |
| Advanced / not-taken | 902 / 191,224 | 966 / 204,792 | 1,606 / 340,472 |
| Advanced / taken | 651 / 138,012 | 779 / 165,148 | 971 / 205,852 |
| Advanced / backward-taken | 651 / 138,012 | 715 / 151,580 | 971 / 205,852 |
| Advanced / one-bit | 653 / 138,436 | 909 / 192,708 | 1,069 / 226,628 |
| Advanced / two-bit | 655 / 138,860 | 719 / 152,428 | 981 / 207,972 |

On the dependent loop, baseline recorded 1,158 `DATA_STALL` observations and
640 `BRANCH_WAIT` observations; these counters can overlap. The advanced core
forwarded dependent data without a data stall. Its not-taken predictor missed
127 of 128 branches, whereas taken and backward-taken each missed once. This
reduced advanced cycles from 902 to 651, yet its 212-tick period still left its
modeled time above single-cycle's 113,344 ticks. On the alternating loop,
backward-taken and two-bit used 715 and 719 cycles, respectively, compared with
966 for not-taken and 909 for one-bit.

The compact timed software has a separate host cost. For the dependent loop,
median host execution was about 0.189 s for single-cycle, 0.468 s for baseline,
and 0.035 s for advanced/taken. These are three-run local software timings;
they do not measure the modeled CPU's clock or hardware throughput. Structural
cross-checks for that loop took roughly 14–24 host seconds per case.

The four complete local artifact directories are
`build/experiments/20260923-binary-safe-22/`,
`build/experiments/20260923-binary-safe-dependent128/`,
`build/experiments/20260923-binary-safe-alternating128/`, and
`build/experiments/20260923-binary-safe-nested32/`. They contain matching source
and binary fingerprints, full clock reports, raw JSONL, CSV/JSON summaries and
`COMPLETE` markers. Together they record **175 structural runs, 175 oracle
runs, 175 warmups and 525 measured runs**; all checks passed. Every measured
row has `modeled_time_ticks = cycles × period_ticks`. The largest recorded
process RSS was 79,868 KiB (about 78 MiB). These workloads use no caches, and
the timing units have no physical-frequency calibration.
