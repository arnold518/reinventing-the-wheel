# RV32I long branch structural rerun

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

All 35 current-source long branch-pattern cases completed with oracle,
expected-result and structural-reference checks. Each of the five campaign
directories has a `COMPLETE` marker. The current-source result is separate
from the September 2026 campaign artifacts.

## 2026-10-14 — Scope and run plan

- The [feasibility review](2026-10-14-rv32i-structural-run-feasibility.md) identified five 64-iteration patterns (`taken`, `not-taken`, `alternating`, `bursts`, `irregular`) and seven targets per pattern: single-cycle, baseline, and advanced with each of five predictor policies.
- Use `build-cleanup/rv32i_experiment` and `tools/rv32i-performance/experiment.py`, `--clock-mode binary-safe`, `--reference-check`, 16 predictor entries, 512 MiB allocation limit, 100,000 cycles, and 100 million events. Allow up to 300 seconds per subprocess. Run serially into new `build/experiments/20261014-branch64-structural-*` directories.
- Use zero host warmups and one measurement repetition because this run is a functional structural comparison, not a new host-performance study. Each case still receives the oracle-checked run, structural reference run, and signature-checked measurement. Do not change or reinterpret the old September result directories.

## 2026-10-14 — Execution and results

- A one-case pilot, `20261014-branch64-structural-pilot-alternating-advanced/`, passed at period 100. Its structural run used 11.23 host seconds and 16,225,129 events. The five full pattern groups then ran serially with the same limits; each returned `Completed 7 cases` and produced a `COMPLETE` marker.
- Each group used `python3 tools/rv32i-performance/experiment.py --binary build-cleanup/rv32i_experiment --output build/experiments/20261014-branch64-structural-PATTERN --workload branch-pattern --iterations 64 --size 8 --pattern PATTERN --cores single,baseline,advanced --policies not-taken,taken,backward-taken,one-bit,two-bit --entries 16 --clock-mode binary-safe --reference-check --allocation-limit-mib 512 --max-cycles 100000 --max-events 100000000 --timeout 300 --warmups 0 --repetitions 1`, substituting `PATTERN` with each of the five named patterns.
- All **35/35** summaries have `run_status=completed` and `structural_checked=true`; all match their independent expected result. There are 105 run records: 35 verified oracle checks, 35 verified structural references and 35 matching measurements. Every phase completed with zero setup violations. A separate artifact audit matched cycles, retired instructions, workload/final-state fingerprints and benchmark result among each three-phase case. Each of the 35 cases has a certified `binary-safe` clock (166 single, 80 baseline, 100 advanced).

| Pattern | Structural checks | Reference host seconds | Reference events | Largest reference RSS | Expected result |
| --- | ---: | ---: | ---: | ---: | ---: |
| Taken | 7/7 | 75.0 | 108.4 million | 60.3 MiB | 0 |
| Not-taken | 7/7 | 74.1 | 106.5 million | 60.4 MiB | 64 |
| Alternating | 7/7 | 79.3 | 114.1 million | 60.6 MiB | 32 |
| Bursts | 7/7 | 75.9 | 108.4 million | 60.3 MiB | 32 |
| Irregular | 7/7 | 77.4 | 111.5 million | 60.4 MiB | 30 |
| **Total** | **35/35** | **381.7** | **548.9 million** | **60.6 MiB maximum** | — |

- The five provenances name Git head `448e4e35093be55f7338e9be5ef456d046ee9d1e`, source SHA-256 `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2` and binary SHA-256 `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1`. A serial `cmake --build build-cleanup --target rv32i_experiment -j1` found the binary up to date; its hash still matched the run provenance.
- All 35 current cycle counts, retirement counts and expected results match the corresponding September 24 branch summaries. Their modeled ticks differ because the certified periods changed. This comparison does not refresh the other 161 core-gap cases, compiled campaigns or timed-memory workloads. One measurement repetition is insufficient for stable host-time comparisons.

## 2026-10-14 — Documentation and verification

- Added the current-source result to public §4.7, including all 35 cycle counts, validation scope, provenance and limits. The September 24 campaign remains a separate historical 199-case result with 164 structural checks; its artifacts were not changed. The open RV32I documentation-update record links this follow-up.
- `RV32IBenchmarkDocumentationTest` passed (1/1). A strict MkDocs build passed using the existing `build-wasm/pages/simulator/example-rows.json`; the rendered result page contains the new section and anchor. `git diff --check` passed. The site build did not rebuild the WebAssembly simulator or run browser tests.
