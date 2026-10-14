# RV32I three-core performance comparison

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

At the final certified FF periods (166/80/100 ticks), advanced/two-bit has the lowest modeled time on all five current-source long branch patterns. The single-cycle core and advanced/not-taken are almost tied on current program 20; baseline is slower there. Broader historical evidence shows advanced/two-bit leading most compiled workloads, but also shows independent arithmetic where baseline wins. The broad compiled and core-gap panels have not been rerun on the current source, so their rankings are historical.

## 2026-10-14 — Current-source evidence

- Re-read [§4.7](../../site-docs/rv32i/experiments.md), the current [long-branch run](2026-10-14-rv32i-branch64-structural-rerun.md), the current program-20 JSON pairs in `build/experiments/20261014-program20-final-clocks/`, and the baseline/advanced architecture pages. The current branch runs have 35/35 oracle, expected-result and structural checks, all at certified 166/80/100-tick FF periods. Each row below is modeled ticks, not host seconds.

| Workload | Single-cycle | Baseline | Advanced/two-bit | Advanced/two-bit reduction vs single / baseline |
| --- | ---: | ---: | ---: | ---: |
| Taken branch pattern | 76,028 | 120,080 | 53,600 | 29.5% / 55.4% |
| Not-taken branch pattern | 86,652 | 125,200 | 59,600 | 31.2% / 52.4% |
| Alternating branch pattern | 81,340 | 122,640 | 62,800 | 22.8% / 48.8% |
| Burst branch pattern | 81,340 | 122,640 | 59,200 | 27.2% / 51.7% |
| Irregular branch pattern | 81,008 | 122,480 | 62,600 | 22.7% / 48.9% |

- The same branch cases retire 457–521 instructions. Single-cycle takes 458–522 cycles, baseline 1,501–1,565, and advanced/two-bit 536–628. Baseline's 80-tick period is shorter than single-cycle's 166, but its approximately threefold cycle count on these branch-heavy programs more than offsets that benefit. Advanced/two-bit removes enough branch and dependency stalls to win despite a 100-tick period.
- Current program 20 at the **same not-taken policy** took 51 × 166 = **8,466** ticks on single-cycle, 138 × 80 = **11,040** on baseline, and 85 × 100 = **8,500** on advanced. Single-cycle is ahead of advanced by only 34 ticks (0.4%); this does not compare advanced's other predictor policies. All three behavioral/reference pairs completed with 50 retired instructions and matching final state.
- Advanced is the only supported core for timed SRAM, DRAM, ROM and NOR-flash in this experiment. Current program-20 results at period 102 range from 334 cycles (NOR-flash) to 583 (DRAM), but these compare memory endpoints on one core, not the three cores against each other.

## 2026-10-14 — Why clock period and cycles must be combined

For the same workload and endpoint, modeled time is `cycles × period`. At the current FF periods, baseline beats single-cycle only if its cycle count is below **2.075×** single-cycle's; advanced beats single-cycle below **1.66×**; advanced beats baseline if its cycle count is below **0.8×** baseline's. On the current long branches, baseline uses 3.0–3.3× single-cycle's cycles, while advanced/two-bit uses about 1.17–1.28×. On current program 20, advanced/not-taken uses 85/51 = 1.667× single-cycle's cycles, just beyond the 1.66 break-even point. These thresholds explain the near tie without treating the shortest clock as the fastest program.

## 2026-10-14 — Historical breadth and counterexamples

- The last completed broad FF fixed/loop panel and the core-gap expansion used an older circuit and **116/62/78-tick** periods. On its eight-iteration independent ALU loop, baseline took **38,750** modeled ticks versus advanced/two-bit **43,758** and single-cycle **63,916**. When dependencies were adjacent, baseline rose to **132,494**, while advanced/two-bit remained **43,758**. The baseline's stall-only dependency handling and control-flow wait explain the workload sensitivity; advanced forwarding and prediction reduce those stalls.
- The historical full Embench campaign completed 19 workloads × seven targets on behavioral cores, with original verifiers and instruction oracle but no structural references. At **116/62/78**, advanced/two-bit had the lowest modeled ticks among the fixed three-target comparison on **16/19** workloads; baseline led `nettle-aes` and `nettle-sha256`, and single-cycle led `nsichneu`. The baseline/advanced `nettle-sha256` difference was only about 0.3%, and the single/advanced `nsichneu` difference about 0.8%. These are workload counts, not a normalized Embench score or a current-source ranking.
- Advanced predictor choice matters. On the current burst pattern, one-bit uses **57,600** ticks versus two-bit **59,200**; on alternating, two-bit uses **62,800** versus one-bit **68,800**. Advanced/not-taken even loses narrowly to single-cycle on the current taken pattern (**78,000** versus **76,028**). Therefore a policy selected after seeing each pattern would exaggerate the core's general performance; the main comparison fixes two-bit.

## 2026-10-14 — Verification and limits

- Recomputed all current branch ratios from the five 2026-10-14 `summary.json` files, checked the current program-20 manifest/JSON pairs, and counted historical full Embench winners from all 133 case summaries. Inspected the September logic/core-gap summaries for independent/dependent workload values. No new simulation, build, CTest or site build was run for this analysis.
- The current-source evidence covers only five branch patterns and one short fixed program across the three FF cores. Structural/reference checks support correctness on those inputs, not universal performance. The 19-workload Embench ranking and independent/dependent examples come from the older source and clock periods; the final 166/80/100 periods cannot be applied to them as measured current results. No area, power or physical-frequency measurements are available here.
