# RV32I Embench core comparison

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

The completed full Embench campaign is a broad **historical** comparison: 19 original workloads × seven targets, 133/133 completed behavioral/oracle/verifier cases, no full structural references, at the older 116/62/78-tick periods. Holding the advanced predictor at two-bit, advanced had the lowest modeled time on 16 of 19 workloads, baseline on two, and single-cycle on one. The current 166/80/100-tick source has not run full Embench; it has only the separate current short branch and program-20 results. The nine structurally checked reduced-work compiled cases support three smaller complete-program comparisons, not structural coverage of the 133 full workloads.

Related [three-core comparison](2026-10-14-rv32i-three-core-performance-comparison.md) and [public results](../../site-docs/rv32i/experiments.md#compiled-workload-results-2026-09-2728).

## 2026-10-14 — Full Embench artifact audit

- `build/experiments/20260927-full-embench/campaign.json` has `status=completed`. All 133 case summaries (19 workloads × single, baseline and five advanced policies) have `run_status=completed` and `structural_checked=false`. The campaign used original workload verifiers and the instruction oracle on behavioral cores. All case summaries record periods 116/62/78 ticks (single/baseline/advanced) and source SHA-256 `3d8092f2a130d85b0109b127eb65a9664f045b580ef9146f6b2cab6111c82d43`.
- For a fixed three-core comparison (single/not-taken, baseline/not-taken, advanced/two-bit), the lowest `cycles × recorded period` belongs to advanced on **16/19** workloads, baseline on `nettle-aes` and `nettle-sha256`, and single-cycle on `nsichneu`. This is a count of workloads, not an official normalized Embench score. The unweighted geometric mean of the 19 per-workload modeled-time speedups is **1.276×** for advanced/two-bit versus single-cycle and **1.478×** versus baseline; these are descriptive ratios on this one historical circuit snapshot.

| Full workload | Single ticks | Baseline ticks | Advanced/two-bit ticks | Lowest |
| --- | ---: | ---: | ---: | --- |
| CRC32 | 711,131,388 | 1,237,913,452 | 546,549,042 | Advanced |
| nettle-aes | 557,646,800 | 364,289,308 | 385,387,860 | Baseline |
| nettle-sha256 | 475,484,116 | 323,621,896 | 324,554,958 | Baseline, by 0.3% versus advanced |
| nsichneu | 259,692,100 | 406,844,558 | 261,687,192 | Single-cycle, by 0.8% versus advanced |

- Among the five advanced predictor policies, two-bit has the lowest modeled ticks on 15 workloads and backward-taken on four (no ties in this artifact). Choosing each workload's best policy after seeing its result is a different comparison; the 16/19 statement keeps two-bit fixed.
- The completed `20260928-middle-equivalence-background/` campaign has **9/9** completed cases with `structural_checked=true`: reduced-work CRC32, SHA-256 and SGLIB × single/baseline/advanced-two-bit. Advanced has the lowest historical modeled ticks on all three. SHA-256 is close: 698,022 advanced versus 701,468 baseline ticks. These are complete invocations of reduced workloads with original verifiers, not the original full Embench inputs. Their source SHA-256 is `ed91061db501df90bfcd77c28dbc876acbe52d954e806afafca0574e1240a669`.

## 2026-10-14 — Final clock question and interpretation

At the current certified FF periods of 166/80/100 ticks, a **clock-only projection** of the old 19 × three cycle counts would put advanced/two-bit first on 18 workloads and baseline first on `nettle-aes`. That calculation assumes every old cycle count remains unchanged after code, delay and clock updates; this has not been checked for full Embench. It is **not** a current-source result and must not replace the historical 16/2/1 measured ranking. The current branch subset's matching cycle counts do not prove that compiled programs also match.

The advanced core's forwarding and prediction are consistent with its lower cycles on many compiled workloads, while the baseline's shorter historical period can win where its extra cycles remain small. Aggregate cycle and tick totals alone do not isolate the cause for every ELF; per-workload stall and prediction observations would be needed for such claims. Host execution time and physical-chip speed are separate from these modeled ticks.

## 2026-10-14 — Verification and limits

- Re-read all 133 full Embench and nine reduced-work case summaries, their parent completion status, period and structural flags; recomputed winners, per-workload ratios, geometric means and predictor-policy winners from the recorded summaries. No new simulation, CTest, build, WebAssembly or site build was run for this read-only analysis.
- Full Embench's 133 cases have behavioral/oracle/original-verifier evidence but zero full structural comparisons. The nine reduced-work structural comparisons cannot close that gap. No current-source full Embench campaign exists; a present-day modeled-time ranking requires a new behavioral run at the final certificates, while full structural verification of all 133 original workloads is not practical as a routine host campaign.
