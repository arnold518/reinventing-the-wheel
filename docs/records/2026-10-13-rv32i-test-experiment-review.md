# RV32I test and experiment evidence review

Started: 2026-10-13. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

The current checks cover component contracts, ISA and program correctness, core integration, finite fidelity comparisons, timing safety, and benchmark infrastructure. Dated campaigns add workload and compiled-program evidence, but were run on older source snapshots. The proposed page work is tracked in the [documentation update](2026-10-13-rv32i-test-experiment-documentation-update.md). No RV32I code or public pages changed for this review.

## 2026-10-13 — Investigation started

- Read `docs/STATUS.md`, `docs/README.md`, the maintained test and benchmark references, public §4.2 verification and experiment pages, `tests/CMakeLists.txt`, `src/tests/TestRegistry.cpp`, and the available CTest inventory.
- The `build-cleanup` tree currently lists 451 CTests. Its fixed-program matrix includes 22 programs across single-cycle, baseline, and five advanced prediction policies (176 registrations); this is a registration count, not a fresh pass result.
- Historical experiment directories include `COMPLETE` markers and incomplete runs. Their source fingerprints, selected periods, and validation phase must be checked before drawing conclusions. A current full suite or new performance campaign has not been run for this investigation.

## 2026-10-13 — Current test inventory and what each layer checks

- `ctest --test-dir build-cleanup --show-only=json-v1` lists 451 registrations: 260 carry the `rv32i` label, 13 the `benchmark` label, and 17 the `timing` label. Labels overlap and are not disjoint totals. The fixed-program matrix is 22 single-cycle, 22 baseline, 22 advanced/default and 22 under each of five advanced predictor policies: 176 registrations. This corrects the earlier abbreviated matrix description, which omitted advanced/default. Registration alone does not establish a pass.
- The tracked [delay and clock record](2026-10-12-behavioral-delays-and-memory-clock-edge.md) reports a clean serial rebuild at `448e4e3` and `ctest --test-dir build-cleanup -j1`: 451/451 passed in 2,404 seconds. This review did not repeat the full suite. A stale `LastTestsFailed.log` from an earlier run is not a current failure result.
- Component tests in `src/tests/` and `src/tests/TestRegistry.cpp` cover ALU/decoder/register/memory contracts, stage and pipeline-register behavior, forwarding and hazards, prediction, four-state inputs, and available structural/behavioral forms. These are local interface and composition checks, not a complete core theorem.
- The ISA decoder/oracle and fixed-program checks cover encodings and independently specified outcomes. Programs 1–8 exercise behavior, 9–16 faults, and 17–22 workloads. Their 650 instruction-attempt checkpoints are not 650 cycles. Program tests compare each core with the instruction oracle and fixture expectations.
- `RV32IDifferentialTest` runs 46 finite behavioral/reference pairs: 12 seeded, 21 fault, 10 predictor, and 3 compiled C cases. It compares settled cycle signals, active buses, stage PCs, retirement, and final state. The structural reference uses compact storage/memory and is not an entirely gate-expanded machine; the paired cases are strong regressions, not formal equivalence.
- `RV32IProfileToggleSweepTest` exercises every selectable contract at least once, not every possible combination. Generated-workload tests check 12 parameterized kernels against host expectations; the public catalogue contains 22 fixed programs plus 15 generated families (including three earlier loops), not 37 independently proven CTest programs.
- ELF loading, the independent `tohost` self-check, experiment CLI, output schema, failure/recovery, budgets and frozen source snapshots have focused tests. `FullEmbenchCampaignTest` explicitly uses tiny smoke ELFs and does not run the 19 real Embench kernels. Default `RV32IMiddleBenchmarkTest` checks the manifest and a stand-in ELF run; real ELF preflight is optional. A passing CTest for either harness is not a passing external suite.
- Safe-clock, binary startup, state/endpoint, and structural-versus-compact clock tests check sufficient periods under declared contracts. The certificate is a model-bound simulation result, not a measured physical Fmax. Browser/WebAssembly checks are a separate layer and were not rerun here.

## 2026-10-13 — Completed historical campaigns

All directories below are under ignored `build/experiments/`; key results are copied here so the evidence does not depend on retaining that build tree. Each inspected provenance file names Git head `c003ed7d` with a dirty worktree; their source hashes differ, and none is the current `448e4e3` checkout. Every listed result has a `COMPLETE` marker and completed case or job metadata. `20260924-core-gap/` is incomplete and excluded.

| Artifact | Completed cases | Actual check | Clock periods (single / baseline / advanced) |
| --- | ---: | --- | --- |
| `20260923-binary-safe-22/` | 154 = 22 programs × 7 targets | Oracle, fixed outcomes, final memory and structural reference on all 154 | 176 / 112 / 212 |
| `20260924-core-gap-rerun/` | 161 = 23 parameter cases × 7 targets | Oracle, host expectations and structural reference on all 161 | 116 / 62 / 78 |
| `20260928-middle-equivalence-background/` | 9 = 3 reduced-work Embench kernels × 3 cores | Original verifier, oracle and full-program structural reference on all 9 | 116 / 62 / 78 |
| `20260927-full-embench/` | 133 = 19 full Embench workloads × 7 targets | Original verifier and oracle on behavioral circuit; `structural_checked=false` on all 133 | 116 / 62 / 78 |
| `20261003-advanced-grouping-clock-final/` | 7 program-20 targets | Structural reference on all 7 | 92 / 58 / 78 |

- The September 24 main panel plus branch/predictor supplements totals 199 cases, 164 structural checks and 1,159 subprocess runs, as recorded in [§4.7](../../site-docs/rv32i/experiments.md). These are different units. Its diagnostic results show forwarding and scheduling effects: at dependency distance 1 the baseline took 2,137 cycles and advanced/two-bit 561; scheduling the tested load-use loop removed 64 bubbles and reduced advanced/two-bit from 292 to 228 cycles. On the tested alias pattern, 8/16 predictor entries produced 67 next-PC misses while 32 entries produced 5. These conclusions are limited to the specified inputs and historical clock model.
- The nine-case middle campaign ran CRC32 (35,907 retired instructions), SHA-256 (8,767) and SGLIB (105,470) with one kernel invocation, no warmup and original verifiers. It compared complete-program architectural signatures, not per-cycle traces or the full-sized Embench workload.
- The 133-case full campaign's 19 workloads are real compiled Embench programs, not the smoke fixtures used by `FullEmbenchCampaignTest`. It is useful evidence of long-running behavioral execution, ISA checking and verifier completion across seven targets. It supplies neither a structural comparison for those full programs nor a current-checkout result or official normalized Embench score.

## 2026-10-13 — Current clock check and conclusions

- This review ran `build-cleanup/rv32i_experiment --safe-clock --binary-startup` for each FF-memory core: single 166, baseline 80 and advanced 100 ticks, all `safe-under-contracts`. It also ran unrestricted-four-state `--safe-clock` for advanced with SRAM, DRAM, ROM and NOR-flash: 102 ticks each, all `safe-under-contracts`. These are clock certificates, not new program-performance campaigns.
- The current delay and memory-edge changes postdate the recorded campaigns. Modeled tick totals at 176/112/212, 116/62/78 or 92/58/78 must not be presented as current results. Timed-memory access latency has also changed by one edge; published timed-memory cycle results need a new run.
- The available tests establish substantial finite correctness and equivalence coverage. They do not establish formal equivalence, exhaustive ISA behavior, physical frequency, cache behavior, or performance of full structural Embench on this checkout.
- No broad campaign or browser/WebAssembly suite was rerun. This completed review is the basis for a staged documentation proposal, not a claim that the public experiment pages are already updated.
