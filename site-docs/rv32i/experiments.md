# § 4.7. RV32I Experiment Results

[Experiment setup](4.2/index.md) · [Fixed programs](4.2/programs/index.md) · [Generated workloads](4.2/workloads/index.md) · [Compiled workloads](4.2/compiled/index.md)

Which of the three RV32I cores of this chapter runs programs fastest: the
[single-cycle core](4.4/index.md), the [baseline pipeline](4.5/index.md) or
the [advanced pipeline](4.6/index.md)? This page runs all three on the
workloads of [§ 4.2](4.2/index.md). The advanced pipeline is run with each of
its five branch predictors, so every comparison has **seven targets**.

All runs use flip-flop memory, which answers in the same clock cycle, so the
results compare the cores and not the memory. How the cores fare with caches
and DRAM is the subject of [§ 3.8 Cache Experiments](../memory/cache-experiments.md).

## The answer in short

- **The advanced pipeline with the two-bit predictor is the fastest of the
  three cores in almost every case**: 13 of 14 fixed programs, 14 of 15
  generated workloads, 20 of 23 controlled tests, all 5 longer branch
  patterns and both compiled programs, in every timing profile.
- **The other cores typically take 1.3–2.1× as long.** The single-cycle core
  needs fewer cycles but each is about 1.75× longer; the baseline pipeline has
  the shortest clock but stalls on every dependency and branch.
- **The baseline wins only when it needs about as many cycles as the
  advanced core** (independent work with few branches); then its shorter
  clock decides.
- **No branch predictor is best everywhere.** Static taken, backward-taken or
  one-bit each win some patterns; two-bit is never far behind and is the
  steadiest choice.

![Five measured FF workload groups, each displaying single-cycle, baseline, and all five advanced policies as modeled-time ratios to the visible single-cycle reference at the embedded clocks](4.7/measured-ff-overview.svg)

*Time of every target against the single-cycle core, per workload group*
(`embedded` clocks; lower is faster). Each point is the geometric mean over
the group's cases of `target time / single-cycle time`. The fixed-program
group leaves out the eight programs that end in an expected trap. The
generated and controlled groups share some cases, so they are separate
descriptions, not parts of one score.

## How to read the results

### The seven targets

| Column | Core | Branch handling |
| --- | --- | --- |
| Single | [Single-cycle](4.4/index.md) | One instruction per cycle: nothing to predict |
| Baseline | [Baseline pipeline](4.5/index.md) | Assumes not taken; stalls on every dependency |
| Adv NT | [Advanced pipeline](4.6/index.md) | Static: always not taken |
| Adv T | Advanced pipeline | Static: always taken |
| Adv BT | Advanced pipeline | Static: backward branches taken, forward not taken |
| Adv 1b | Advanced pipeline | One-bit history per branch (16 entries) |
| Adv 2b | Advanced pipeline | Two-bit counter per branch (16 entries) |

### Cycles and time

The tables give **cycles**. A core's cycle count is the same in every
[timing profile](../timing.md#timing-profiles): the campaigns were run in
`unit` and again in `embedded` and `desktop`, and every case took the same
number of cycles. What differs is the length of a cycle, each core's
certified clock period:

<!-- profile-table: clocks -->

**Time = cycles × clock period**, so a core with more cycles can still win
with a shorter clock. The advanced core's period is the same for all five
predictors. The single-cycle period is 1.75× the advanced one in `embedded`
(1.74× in `desktop`, 1.77× in `unit`).

<a id="completed-campaign-coverage"></a>

### How the runs were checked

Every run matched the instruction oracle (or the fixture's own verdict) and
the expected final memory, with zero setup violations at its clock. Most were
also compared with a structural reference core run at the same clock ([equivalence checks](4.2/verification/equivalence.md)): all fixed
programs, longer branch patterns, predictor sizes and compiled programs, and
a spot check of the generated workloads.

| Workload group | Cases per profile | Results |
| --- | ---: | --- |
| 22 fixed programs × 7 targets | 154 | [Fixed programs](#fixed-programs) |
| 15 generated workloads × 7 targets | 105 | [Generated workloads](#generated-workloads) |
| 23 controlled tests × 7 targets | 161 | [Controlled tests](#controlled-tests) |
| 5 longer branch patterns × 7 targets | 35 | [Longer branch patterns](#longer-branch-patterns) |
| 3 predictor sizes, advanced two-bit | 3 | [Predictor size](#predictor-size) |
| 2 compiled programs × 7 targets | 14 | [Compiled programs](#compiled-programs) |

Cases count table entries, not distinct programs: some workloads appear in
more than one group. The [full Embench section](#full-embench-conditional-clock-only-projection)
is a projection from older cycle counts, outside these totals.

<a id="current-clock-certificates-2026-10-13"></a>
<a id="cross-group-comparison"></a>

## Summary by workload group

The advanced core fixed to two-bit prediction against the other two cores,
on exactly the same cases. "Takes" is the geometric mean of
`other core's time / advanced two-bit time` in `embedded`; above 1 means
slower than advanced.

| Group | What it tests | Cases | Fastest core: single / baseline / advanced | Single-cycle takes | Baseline takes |
| --- | --- | ---: | ---: | ---: | ---: |
| [Fixed programs](#fixed-programs) | Short programs and instruction mixes | 14 | 0 / 1 / 13 | 1.33× | 1.33× |
| [Generated workloads](#generated-workloads) | One case of each workload family | 15 | 0 / 1 / 14 | 1.41× | 1.73× |
| [Controlled tests](#controlled-tests) | One pressure varied at a time | 23 | 0 / 3 / 20 | 1.45× | 1.65× |
| [Longer branch patterns](#longer-branch-patterns) | Five branch patterns, 64 iterations | 5 | 0 / 0 / 5 | 1.44× | 2.11× |

The winner counts are the same in all three profiles. In `desktop` the
ratios match `embedded` within 0.001; in `unit` they are 1–4 % higher,
because the baseline and single-cycle clocks are relatively a little longer
there. The groups share some cases, so the rows are separate descriptions,
not scores to average.

## Results by workload group

### Fixed programs

The [22 fixed programs](4.2/programs/index.md) test instruction behavior and
include six short workloads. Programs 09–16 end in an **expected trap or
fault**: their tiny counts test that a core stops at the right instruction,
not its speed, so the comparisons below use the other 14.

**Cycles, complete 22 × 7 FF panel:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [program-01](4.2/programs/program-01.md) | 29 | 72 | 43 | 39 | 39 | 41 | 43 |
| [program-02](4.2/programs/program-02.md) | 35 | 50 | 39 | 39 | 39 | 39 | 39 |
| [program-03](4.2/programs/program-03.md) | 17 | 27 | 21 | 21 | 21 | 21 | 21 |
| [program-04](4.2/programs/program-04.md) | 24 | 56 | 40 | 30 | 40 | 40 | 40 |
| [program-05](4.2/programs/program-05.md) | 11 | 25 | 19 | 19 | 19 | 19 | 19 |
| [program-06](4.2/programs/program-06.md) | 10 | 20 | 14 | 14 | 14 | 14 | 14 |
| [program-07](4.2/programs/program-07.md) | 61 | 123 | 79 | 67 | 67 | 69 | 71 |
| [program-08](4.2/programs/program-08.md) | 42 | 89 | 59 | 53 | 53 | 55 | 57 |
| [program-09](4.2/programs/program-09.md) | 2 | 6 | 6 | 6 | 6 | 6 | 6 |
| [program-10](4.2/programs/program-10.md) | 2 | 6 | 6 | 6 | 6 | 6 | 6 |
| [program-11](4.2/programs/program-11.md) | 2 | 9 | 6 | 6 | 6 | 6 | 6 |
| [program-12](4.2/programs/program-12.md) | 3 | 10 | 7 | 7 | 7 | 7 | 7 |
| [program-13](4.2/programs/program-13.md) | 2 | 9 | 6 | 6 | 6 | 6 | 6 |
| [program-14](4.2/programs/program-14.md) | 3 | 10 | 7 | 7 | 7 | 7 | 7 |
| [program-15](4.2/programs/program-15.md) | 2 | 9 | 6 | 6 | 6 | 6 | 6 |
| [program-16](4.2/programs/program-16.md) | 3 | 12 | 9 | 9 | 9 | 9 | 9 |
| [program-17](4.2/programs/program-17.md) | 73 | 77 | 77 | 77 | 77 | 77 | 77 |
| [program-18](4.2/programs/program-18.md) | 66 | 262 | 70 | 70 | 70 | 70 | 70 |
| [program-19](4.2/programs/program-19.md) | 67 | 169 | 103 | 103 | 103 | 103 | 103 |
| [program-20](4.2/programs/program-20.md) | 51 | 138 | 85 | 57 | 57 | 59 | 61 |
| [program-21](4.2/programs/program-21.md) | 84 | 217 | 134 | 106 | 106 | 108 | 110 |
| [program-22](4.2/programs/program-22.md) | 61 | 129 | 87 | 75 | 75 | 77 | 79 |

What it shows:

- **Advanced two-bit is fastest on 13 of the 14 programs.** The exception is
  program 17: baseline and advanced both need 77 cycles, and the baseline's
  shorter clock wins (258 ns against 316 ns in `embedded`).
- **Fewer cycles do not mean less time.** Program 04 takes 24 single-cycle
  cycles against 40 advanced two-bit cycles, yet advanced is slightly faster
  (164 ns against 172 ns in `embedded`).
- **Two-bit is not the best predictor on every program.** On program 20,
  static taken and backward-taken need 57 cycles against 61 for two-bit and 85
  for not-taken.
- With the eight trap programs included, the single-cycle core comes within
  2 % of advanced two-bit: in such short runs, filling the pipeline costs more
  than the pipeline saves.

<a id="memory-endpoint-follow-up"></a>
<a id="default-policy-program-20-check"></a>
<a id="generated-families"></a>

### Generated workloads

The [15 generated workload families](4.2/workloads/index.md) each contribute
one case, run unchanged on all seven targets. The three legacy loops use their
own iteration scheme; the other twelve run eight iterations with size eight
and seed one (sort and CRC32: two iterations). A family name stands for this
one case, not for every parameter value.

**Cycles, complete 15 × 7 FF panel:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [dependent](4.2/workloads/dependent.md) | 44 | 142 | 62 | 50 | 50 | 52 | 54 |
| [alternating](4.2/workloads/alternating.md) | 40 | 130 | 66 | 54 | 54 | 64 | 58 |
| [nested](4.2/workloads/nested.md) | 220 | 603 | 350 | 242 | 242 | 260 | 250 |
| [independent](4.2/workloads/independent.md) | 551 | 625 | 569 | 557 | 557 | 559 | 561 |
| [dependency](4.2/workloads/dependency.md) | 551 | 2,137 | 569 | 557 | 557 | 559 | 561 |
| [load-use](4.2/workloads/load-use.md) | 218 | 468 | 300 | 288 | 288 | 290 | 292 |
| [load-scheduled](4.2/workloads/load-scheduled.md) | 218 | 404 | 236 | 224 | 224 | 226 | 228 |
| [branch-pattern](4.2/workloads/branch-pattern.md) | 70 | 217 | 104 | 92 | 92 | 100 | 96 |
| [branch-alias](4.2/workloads/branch-alias.md) | 161 | 558 | 195 | 183 | 183 | 201 | 187 |
| [calls](4.2/workloads/calls.md) | 170 | 434 | 268 | 256 | 256 | 258 | 260 |
| [stream](4.2/workloads/stream.md) | 519 | 1,156 | 713 | 605 | 605 | 623 | 613 |
| [pointer-chase](4.2/workloads/pointer-chase.md) | 391 | 1,220 | 649 | 541 | 541 | 559 | 549 |
| [sort](4.2/workloads/sort.md) | 453 | 1,166 | 615 | 679 | 567 | 593 | 585 |
| [crc32](4.2/workloads/crc32.md) | 823 | 2,179 | 1,233 | 1,001 | 1,017 | 1,057 | 1,039 |
| [scan](4.2/workloads/scan.md) | 439 | 1,244 | 633 | 525 | 525 | 543 | 533 |

What it shows:

- **Advanced two-bit is fastest on 14 of 15.** The exception is
  `independent`: baseline needs 625 cycles against 561, but its shorter clock
  makes it faster (2.10 µs against 2.30 µs in `embedded`).
- **Dependencies are what the baseline cannot hide.** `dependency` does the
  same work as `independent` with each instruction waiting for the last:
  single-cycle and advanced keep their 551 and 561 cycles, while baseline
  rises from 625 to 2,137, because only the advanced core forwards results.
- **Scheduling loads helps.** Moving work between a load and its use lowers
  advanced two-bit from 292 to 228 cycles and removes all 64 load-use stalls.
- **Static taken and backward-taken often beat two-bit** on these short
  cases, before a two-bit counter has learned the branches.

<a id="controlled-core-gap-cases"></a>

### Controlled tests

The [core-gap preset](4.2/campaigns.md) changes one pressure at a time and
keeps everything else identical: dependency distance, load scheduling, branch
direction, branch aliasing, predictor training, calls and data kernels. Some
settings repeat a generated case. They are diagnostics, not 23 independent
programs. Sort and CRC32 run two iterations; the other whole programs eight,
with size eight and seed one.

#### Dependencies and loads

**Cycles, eight controlled settings:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| independent-short | 89 | 128 | 93 | 95 | 95 | 93 | 93 |
| independent-long | 551 | 625 | 569 | 557 | 557 | 559 | 561 |
| dependency-d1 | 551 | 2,137 | 569 | 557 | 557 | 559 | 561 |
| dependency-d2 | 551 | 1,121 | 569 | 557 | 557 | 559 | 561 |
| dependency-d4 | 551 | 625 | 569 | 557 | 557 | 559 | 561 |
| dependency-d8 | 551 | 625 | 569 | 557 | 557 | 559 | 561 |
| load-use | 218 | 468 | 300 | 288 | 288 | 290 | 292 |
| load-scheduled | 218 | 404 | 236 | 224 | 224 | 226 | 228 |

In `dN` a result is used N instructions after it is produced. At distance one the
baseline needs 2,137 cycles against advanced two-bit's 561; at distance four
or more it is back to 625 cycles and, with its shorter clock, faster than
advanced. The load-use pair is the same work with and without scheduling:
292 cycles and 64 load-use stalls against 228 and none.

![Two measured FF contrasts, dependency against independent work and load-use against scheduled loads, with all seven targets visible in each](4.7/measured-core-gap-contrasts.svg)

*Cost of a dependency and of an unscheduled load.* Each bar is the stressed
case's cycles divided by its control's (above one: the pressure costs
cycles), for all seven targets.

#### Branches

**Cycles, nine controlled settings:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| branch-taken | 66 | 213 | 108 | 80 | 96 | 84 | 88 |
| branch-not-taken | 74 | 221 | 100 | 104 | 88 | 90 | 92 |
| branch-alternating | 70 | 217 | 104 | 92 | 92 | 100 | 96 |
| branch-bursts | 74 | 221 | 100 | 104 | 88 | 90 | 92 |
| branch-irregular | 69 | 216 | 105 | 89 | 93 | 95 | 99 |
| alias-spacing8 | 97 | 302 | 131 | 119 | 119 | 107 | 111 |
| alias-spacing16 | 161 | 558 | 195 | 183 | 183 | 201 | 187 |
| branch-body-training0 | 56 | 184 | 94 | 66 | 82 | 70 | 74 |
| branch-body-training32 | 56 | 184 | 94 | 66 | 82 | 66 | 66 |

The branch direction decides which predictor wins. The training pair
measures the same branch body after 0 or 32 earlier iterations (executed and
checked, but outside the measured window): a trained two-bit predictor drops
from 74 to 66 cycles and from five next-PC misses to one.

#### Calls and data kernels

**Cycles, six controlled settings:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| calls | 170 | 434 | 268 | 256 | 256 | 258 | 260 |
| stream | 519 | 1,156 | 713 | 605 | 605 | 623 | 613 |
| pointer-chase | 391 | 1,220 | 649 | 541 | 541 | 559 | 549 |
| scan | 439 | 1,244 | 633 | 525 | 525 | 543 | 533 |
| sort | 453 | 1,166 | 615 | 679 | 567 | 593 | 585 |
| crc32 | 823 | 2,179 | 1,233 | 1,001 | 1,017 | 1,057 | 1,039 |

These add variety to the controlled group: calls and data-heavy loops on
the same seven targets, each for its stated input only.

### Longer branch patterns

The five branch directions again, now for 64 iterations of the
[branch-pattern](4.2/workloads/branch-pattern.md) workload, so that the
predictors have time to learn.

**Cycles, 64 iterations of each pattern:**

| Target | taken | not-taken | alternating | bursts | irregular |
| --- | ---: | ---: | ---: | ---: | ---: |
| Single | 458 | 522 | 490 | 490 | 488 |
| Baseline | 1,501 | 1,565 | 1,533 | 1,533 | 1,531 |
| Adv NT | 780 | 716 | 748 | 748 | 750 |
| Adv T | 528 | 720 | 624 | 624 | 618 |
| Adv BT | 656 | 592 | 624 | 624 | 626 |
| Adv 1b | 532 | 594 | 688 | 576 | 626 |
| Adv 2b | 536 | 596 | 628 | 592 | 626 |

![Measured modeled time for five longer branch patterns across all seven targets, with each row's fastest value outlined](4.7/measured-long-branches.svg)

*Time of each target per pattern* (`embedded` clocks). Color shows each value
against the pattern's fastest target; outlines mark the fastest. Compare
within a pattern: the patterns do different amounts of work.

What it shows:

- **Every advanced predictor beats both other cores on every pattern.**
- **Each pattern has its own best predictor:** taken wins always-taken and
  irregular, backward-taken wins always-not-taken, taken and backward-taken
  tie on alternating, one-bit wins bursts.
- **Two-bit is the safe choice:** within 2.8 % of the best on every pattern,
  and the lowest geometric mean of the five predictors.

<a id="predictor-capacity"></a>

### Predictor size

One 64-iteration case whose branches are 16 addresses apart, run on advanced
two-bit with three table sizes:

| Entries | Cycles | Next-PC misses |
| ---: | ---: | ---: |
| 8 | 1,363 | 67 |
| 16 | 1,363 | 67 |
| 32 | 1,239 | 5 |

With 8 or 16 entries the branches collide in the table; 32 entries separate
them and save 9.1 % of the time. This shows the effect of aliasing on one
address pattern, not a best table size.

<a id="compiled-fixtures"></a>

### Compiled programs

Two [checked-in ELF programs](4.2/compiled/index.md#checked-in-fixtures)
test loading compiler-produced RV32I code. Both passed their own verdict, the
oracle and the structural reference on all seven targets.

**Cycles, complete 2 × 7 FF panel:**

| Case | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| self-check.elf | 19 | 56 | 26 | 32 | 26 | 26 | 26 |
| compiled.elf | 64 | 166 | 86 | 94 | 86 | 88 | 88 |

Advanced two-bit is the fastest core on both, in every profile. Both are
small and dominated by startup code; they are not the full applications of
the next section.

### Full Embench: conditional clock-only projection

**Not a current measurement.** An earlier campaign ran **19 Embench
workloads** on all seven targets with their original verifiers and the
instruction oracle, but without the structural reference, on an older version
of the code. They were not rerun. The table holds those historical cycle
counts; the times below assume they are unchanged and multiply them by the
current clocks.

**Historical cycles, all 19 × 7 target cells:**

| Workload | Single | Baseline | Adv NT | Adv T | Adv BT | Adv 1b | Adv 2b |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [aha-mont64](4.2/compiled/aha-mont64.md) | 11,425,502 | 16,739,060 | 14,019,466 | 12,357,832 | 12,564,698 | 12,380,704 | 12,312,016 |
| [crc32](4.2/compiled/crc32.md) | 6,130,443 | 19,966,346 | 7,356,891 | 7,007,039 | 7,007,033 | 7,007,375 | 7,007,039 |
| [cubic](4.2/compiled/cubic.md) | 30,287,860 | 71,399,587 | 42,590,615 | 37,505,317 | 37,296,609 | 37,130,343 | 36,513,847 |
| [edn](4.2/compiled/edn.md) | 75,135,316 | 191,060,158 | 111,910,206 | 92,733,864 | 89,051,016 | 83,631,098 | 83,518,500 |
| [huffbench](4.2/compiled/huffbench.md) | 2,693,952 | 5,749,891 | 3,365,737 | 3,215,457 | 2,922,743 | 2,963,505 | 2,940,475 |
| [matmult-int](4.2/compiled/matmult-int.md) | 29,506,443 | 74,426,421 | 44,177,157 | 36,564,643 | 36,015,495 | 37,160,367 | 36,407,605 |
| [minver](4.2/compiled/minver.md) | 12,751,619 | 29,064,911 | 17,987,531 | 16,165,139 | 15,762,493 | 15,306,379 | 15,227,437 |
| [nbody](4.2/compiled/nbody.md) | 27,246,935 | 65,541,011 | 38,868,841 | 33,659,173 | 32,939,523 | 33,132,073 | 32,826,229 |
| [nettle-aes](4.2/compiled/nettle-aes.md) | 4,807,300 | 5,875,634 | 5,044,530 | 5,046,090 | 4,957,130 | 4,998,206 | 4,940,870 |
| [nettle-sha256](4.2/compiled/nettle-sha256.md) | 4,099,001 | 5,219,708 | 4,206,627 | 4,177,117 | 4,158,071 | 4,168,539 | 4,160,961 |
| [nsichneu](4.2/compiled/nsichneu.md) | 2,238,725 | 6,562,009 | 3,379,608 | 4,175,512 | 3,377,174 | 3,406,714 | 3,354,964 |
| [picojpeg](4.2/compiled/picojpeg.md) | 5,241,766 | 11,818,952 | 6,129,158 | 5,591,750 | 5,768,474 | 5,537,090 | 5,519,994 |
| [qrduino](4.2/compiled/qrduino.md) | 6,032,091 | 15,169,390 | 7,948,406 | 7,543,352 | 7,374,356 | 7,556,310 | 7,317,850 |
| [sglib-combined](4.2/compiled/sglib-combined.md) | 2,975,542 | 6,906,791 | 3,828,677 | 4,063,561 | 3,808,805 | 3,698,971 | 3,637,847 |
| [slre](4.2/compiled/slre.md) | 2,533,787 | 5,732,802 | 3,104,696 | 3,452,584 | 3,104,482 | 3,061,616 | 2,969,702 |
| [st](4.2/compiled/st.md) | 17,895,271 | 42,991,287 | 25,458,239 | 22,453,967 | 21,590,901 | 21,850,025 | 21,484,209 |
| [statemate](4.2/compiled/statemate.md) | 1,684,582 | 3,023,302 | 1,938,243 | 1,875,385 | 1,914,555 | 1,887,029 | 1,855,627 |
| [ud](4.2/compiled/ud.md) | 6,771,211 | 14,687,130 | 8,966,161 | 9,051,945 | 8,658,491 | 8,868,513 | 8,531,311 |
| [wikisort](4.2/compiled/wikisort.md) | 6,310,064 | 14,142,504 | 8,959,126 | 8,149,074 | 7,926,686 | 7,644,544 | 7,552,916 |

![Conditional full Embench projection for 19 workloads and all seven targets, using older cycles multiplied by current FF periods rather than a current-source rerun](4.7/conditional-embench-projection.svg)

*Projected time against the single-cycle core* (`embedded` clocks), from
the historical cycles above; valid only if those counts still hold.

If the old cycle counts still hold, advanced two-bit would be the fastest
core on all 19 workloads in `unit` and on 18 of 19 in `embedded` and
`desktop`, where baseline would lead `nettle-aes`. Single-cycle would take
1.52× (`unit`) and 1.50× (`embedded`, `desktop`) as long as advanced two-bit,
and baseline 1.58× and 1.52× (geometric means). This is arithmetic on old
counts, not a validated comparison or an official Embench score.

<a id="memory-topologies-and-caches"></a>
<a id="interpretation-and-evidence-limits"></a>

## What these results do not show

- **Memory.** All results use flip-flop memory. The latency of every memory
  technology is in [3.6 Memory Timing](../memory/timing.md#measured-latency),
  and what caches do over DRAM in [3.8 Cache Experiments](../memory/cache-experiments.md).
  An older campaign with timed memories used the controllers from before the
  gate-level memory timing work; it was not rerun and remains in the
  repository history at tag `timing-legacy`.
- **Other inputs.** The tables cover the stated programs, sizes, predictor
  sizes and clocks. Any change to the circuits, delay tables or clocks needs
  new certificates and, where cycles can change, new runs.
- **Host speed.** Results are simulated cycles and times; how long the host
  computer took is not a measured result.
- **Earlier publication.** The first `unit` campaign (October 14, 2026) ran
  the advanced core at 100 ticks, the certificate then; the current one is 94.
  The cycles are unchanged, but the shorter clock changes a few orderings:
  single-cycle no longer wins two fixed programs, and every advanced
  predictor now beats single-cycle on every longer branch pattern.

<a id="measured-result-binary-initialized-clocks"></a>
<a id="expanded-workload-results-2026-09-24"></a>
<a id="compiled-workload-results-2026-09-2728"></a>
