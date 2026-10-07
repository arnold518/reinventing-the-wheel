# Expanded RV32I performance workloads, 2026-09-24

This milestone changes experiment programs and tooling, not core circuitry,
memory devices, primitive delays, timing proof rules, or the existing 22
numbered visualizer scenarios. The benchmark contract and commands are documented
in §4.7 (`site-docs/rv32i/experiments.md`).

## Implementation

- `RV32IBenchmarks` generates twelve new workload families with bounded code
  size, label/range-checked instruction encoding, deterministic input images,
  independently computed results, and output-memory expectations.
- The experiment harness loads these images and checks their intended algorithm
  as well as instruction-oracle agreement. All programs use RV32I only.
- Automatic outer-body regions support a real training prefix at the same PCs;
  measured iterations exclude that prefix, while complete execution is checked.
- The `core-gap` campaign has 23 parameterized cases, leaves correctness/fault
  programs out of performance summaries, and checks identical input images,
  final states, and retirement counts across core configurations.
- CSV/JSON summaries expose parameters, regions, expected/actual results and
  overlapping stall/prediction observations. They do not present those counters
  as an additive CPI breakdown.

The load scheduling pair has exactly the same instruction multiset and input
data. Sort restores its unsorted source on every iteration. Pointer chasing
uses a seeded ring visiting every node once per iteration. CRC is an ordinary
bitwise RV32I kernel, not an official external benchmark implementation/score.

## Focused validation

Serial CTest run: `RV32IBenchmarkTest`, `RV32IBenchmarkCLITest`,
`RV32IExperimentTest`, and `RV32IExperimentCLITest`: **4/4 passed**.

- 95 generated-image/oracle cases, including minimum/maximum sizes, alternate
  seeds, dependency distances, branch patterns, alias spacing and training.
- Each of twelve new families executes on all three cores; all 36 combinations
  match between compact and structural-reference implementations at the current
  certified clocks, with independently checked results.
- Reordering the load-use pair preserves work and removes exactly 32 advanced
  load-use bubbles in the focused four-iteration/eight-word test.
- Region tests check exact measured instruction counts, identical complete
  images/results for equivalent whole versus prefix-trained executions, and
  structural/behavioral counter agreement.
- CLI rejection, campaign output, and existing experiment regressions pass.

The full repository test suite was not run.

## Completed main panel

Artifact: `build/experiments/20260924-core-gap-rerun/` (`COMPLETE`).
The earlier `20260924-core-gap/` directory is an interrupted partial run from an
environment/session change, not part of the results.

All **161 cases** passed oracle and independent-result checks, structural versus
behavioral comparison, and cross-core final-image/retirement-count checks. Each
has one host warmup and three measured repetitions, with a fresh cold predictor
per process. Parameters: eight outer iterations, eight data elements, seed 1;
sort/CRC use two outer iterations and the short arithmetic case uses one.
Five predictor policies use 16 entries. The clocks remain 116/62/78 ticks for
single/baseline/advanced. Every native subprocess has a 512 MiB address-space
limit; runs and compilation are serial.

Below are modeled execution ticks, **not host runtime or physical timing**.
The raw summary retains all seven configurations; this table uses fixed
not-taken/two-bit advanced configurations rather than picking a winner per row.

| Case | Single | Baseline | Advanced / not-taken | Advanced / two-bit |
| --- | ---: | ---: | ---: | ---: |
| Independent, short | 10,324 | 7,936 | 7,254 | 7,254 |
| Independent, long | 63,916 | 38,750 | 44,382 | 43,758 |
| Dependency distance 1 | 63,916 | 132,494 | 44,382 | 43,758 |
| Dependency distance 2 | 63,916 | 69,502 | 44,382 | 43,758 |
| Dependency distance 4 | 63,916 | 38,750 | 44,382 | 43,758 |
| Dependency distance 8 | 63,916 | 38,750 | 44,382 | 43,758 |
| Immediate load-use | 25,288 | 29,016 | 23,400 | 22,776 |
| Scheduled load-use | 25,288 | 25,048 | 18,408 | 17,784 |
| Taken branch | 7,656 | 13,206 | 8,424 | 6,864 |
| Not-taken branch | 8,584 | 13,702 | 7,800 | 7,176 |
| Alternating branch | 8,120 | 13,454 | 8,112 | 7,488 |
| Burst branch, first eight outcomes | 8,584 | 13,702 | 7,800 | 7,176 |
| Irregular branch, first eight outcomes | 8,004 | 13,392 | 8,190 | 7,722 |
| Alias spacing 8 words | 11,252 | 18,724 | 10,218 | 8,658 |
| Alias spacing 16 words | 18,676 | 34,596 | 15,210 | 14,586 |
| Calls/returns | 19,720 | 26,908 | 20,904 | 20,280 |
| Streaming copy | 60,204 | 71,672 | 55,614 | 47,814 |
| Pointer chasing | 45,356 | 75,640 | 50,622 | 42,822 |
| Byte scan | 50,924 | 77,128 | 49,374 | 41,574 |
| Insertion sort | 52,548 | 72,292 | 47,970 | 45,630 |
| CRC-32 | 95,468 | 135,098 | 96,174 | 81,042 |
| Taken branch body, no training | 6,496 | 11,408 | 7,332 | 5,772 |
| Taken branch body, 32 prefix iterations | 6,496 | 11,408 | 7,332 | 5,148 |

Interpretation:

- Baseline's shorter clock wins when dependencies are well separated. It loses
  badly on adjacent consumers because it waits instead of forwarding. Advanced
  takes the same 561 cycles across all four dependency distances with two-bit
  prediction, while baseline ranges from 625 to 2,137 cycles.
- The load schedules retire the same number of instructions and produce the
  same result. Advanced's 64 load-use stalls become zero, reducing two-bit
  execution from 292 to 228 cycles. This is an instruction-order effect, not a
  different algorithm or memory device.
- Calls use repeated JALR returns/indirect transfers; no BTB/RAS was added.
  Advanced/two-bit remains 2.84% slower than single-cycle on this case.
- Prefix training reduces the two-bit body region from 74 to 66 cycles and
  observed next-PC misses from five to one, with 56 retirements in both regions.
  Full-execution results include the prefix; regions count only measured work.
- Eight branch iterations do not span the 32-outcome pattern. In particular,
  the burst row here sees only its first eight not-taken outcomes; do not use it
  as evidence about pattern transitions. Longer branch sweeps are separate.
- Comparing the two alias spacings directly is not a pure table-size test:
  padding instructions also differ. The supplemental capacity sweep fixes the
  program/spacing and varies only predictor entries.

Maximum RSS over measured main-panel runs was 57,028 KiB. Structural validation
runs have their own RSS in `runs.jsonl`; that measured-run figure is not a build
or whole-campaign memory bound.

## Full-pattern and table-capacity checks

Five 64-iteration branch-pattern campaigns cover each 32-outcome sequence twice.
These 35 larger cases use compact cores with instruction-oracle and independent
result checking; they do **not** repeat structural execution. Structural checks
cover the small main-panel cases. All configurations use the current certified
clock and the same bytes for a given pattern.

| Pattern | Single | Baseline | Adv. not-taken | Adv. taken | Adv. backward-taken | Adv. one-bit | Adv. two-bit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Taken | 53,128 | 93,062 | 60,840 | 41,184 | 51,168 | 41,496 | 41,808 |
| Not-taken | 60,552 | 97,030 | 55,848 | 56,160 | 46,176 | 46,332 | 46,488 |
| Alternating | 56,840 | 95,046 | 58,344 | 48,672 | 48,672 | 53,664 | 48,984 |
| Bursts | 56,840 | 95,046 | 58,344 | 48,672 | 48,672 | 44,928 | 46,176 |
| Irregular | 56,608 | 94,922 | 58,500 | 48,204 | 48,828 | 48,828 | 48,828 |

These are modeled ticks. Pattern names describe the data-dependent **forward**
branch; every iteration also executes the mostly-taken backward loop branch.
Miss observations include both. One-bit changes direction more quickly at burst
boundaries (9 total misses versus two-bit's 17), while two-bit is less disrupted
by this alternating sequence (35 versus one-bit's 65). These examples explain
mechanisms; they do not establish a universal predictor ranking.

The capacity sweep keeps exactly the same image, 64 iterations, seed 1, and
16-word spacing, varying only the two-bit predictor's entry count. It includes
structural-reference checks for all three configurations. Branch PCs are
`0x10`, `0x50`, and the loop branch at `0x5C`.

| Entries | Sufficient period | Cycles | Next-PC misses | Modeled ticks |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 78 | 1,363 | 67 | 106,314 |
| 16 | 78 | 1,363 | 67 | 106,314 |
| 32 | 78 | 1,239 | 5 | 96,642 |

The first two branch PCs collide at 8/16 entries but separate at 32. Moving to
32 entries saves 124 cycles here. All three source-derived clock certificates
happen to be 78 ticks because another path remains limiting. This does not
imply that larger tables are free in area/power or will always keep that period.

Artifacts: `build/experiments/20260924-branch64-{pattern}/` and
`build/experiments/20260924-alias64-entries{8,16,32}/`, all with `COMPLETE` markers.
Reproduce branch sweeps with `--workload branch-pattern --pattern NAME
--iterations 64 --size 8` and table sweeps with `--workload branch-alias
--branch-spacing 16 --iterations 64 --size 8 --cores advanced --policies two-bit
--entries N --reference-check`, using the common binary-safe, 512 MiB, one
warmup/three repetition options from the main panel.

Across the main and supplemental campaigns: **199 cases, 164 structural checks,
1,159 recorded runs**. All passed. Maximum reported RSS across recorded runs was
87,908 KiB (85.8 MiB), excluding builds/documentation. The initial interrupted
campaign is excluded. No circuit optimization was made during this experiment
expansion.

The strict MkDocs build passed after recording the results. The generated
GitHub Pages site at `build-wasm/site/` includes the updated §4.7 and the existing
verified simulator package. No WASM rebuild or layout recalculation was needed:
these additions are headless benchmark tools, with no changed visualizer
scenarios or circuit topology.
