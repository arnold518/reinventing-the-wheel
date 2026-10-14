# RV32I results presentation

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

The public §4.7 report now has separate core, predictor, timed-memory and conditional Embench projection charts. Direct run-artifact references belong in this internal record. This follows the [completed §4.7 rewrite](2026-10-13-rv32i-test-experiment-documentation-update.md). The measured results and conditional Embench projection remain distinct.

## 2026-10-14 — Evidence and decision

- The public page currently has one chart for four matched FF core panels. Its predictor and memory analyses have tables but no visual summary. A five-pattern × five-policy heatmap can show each advanced predictor policy's modeled-tick overhead relative to the best policy on that pattern. A three-program × four-endpoint heatmap can show timed-memory ticks relative to FF for programs 19–21, which are compatible with all endpoint choices. Program 22 has no read-only endpoint pair and must stay outside that matched chart.
- The completed predictor inputs are `build/experiments/20261014-branch64-structural-{taken,not-taken,alternating,bursts,irregular}/summary.json`, each seven rows with `COMPLETE`, oracle result and structural checks. The completed memory inputs are `build/experiments/20261014-nonembench-full/timed-memory/summary.json`, 18 rows with `COMPLETE`. Both have source SHA-256 `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2` and binary SHA-256 `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1`. These fingerprints match the existing core chart's inputs.
- Keep chart inputs, source fingerprints, artifact paths and generation commands in this record and the plotting code. The public report should describe scope, method, results and limits without direct links to experiment evidence files or SHA-256 values. The existing full Embench section remains a clock-only projection from older cycles.
- The 19-row full Embench table also benefits from a separate relative-ticks dot plot. Its input is `build/experiments/20261014-nonembench-full/embench-clock-projection.csv` (19 workloads × seven targets). The plot compares projected single-cycle and baseline ticks with projected advanced/two-bit ticks for each workload. It must state that the historical cycles were not rerun.

## 2026-10-14 — Documentation and chart changes

- Extended `tools/rv32i-performance/plot-current-results.py` so `--all` generates core, predictor, memory and Embench-projection SVG charts. The predictor chart shows percent overhead relative to the best policy per branch pattern; two-bit stays within 2.8% of the best in each of five patterns. The memory chart shows endpoint ticks divided by same-program FF ticks for programs 19–21; observed ratios range from 3.99× (NOR-flash on program 19) to 8.48× (DRAM on program 20). Program 22 remains in the table but is absent from the matched chart because the read-only pairings were not run. The Embench projection chart makes its clock-only, older-cycle basis explicit.
- The script validates completed predictor and memory inputs, their source/binary fingerprints, periods and tick arithmetic. It checks that the timed-memory rows have the expected compatible program/endpoint set and agree on final state within each program. For the projection it checks all 133 rows and cycle × period arithmetic; it does not assert that old cycles are current-source measurements.
- Removed source and binary SHA-256 values, `build/` evidence paths, raw JSON/CSV manifest references and the visible legacy-record note from the public §4.7 page. It retains results, scope, caveats and invisible legacy anchors used by existing internal links. No simulator behavior or benchmark result changed.

## 2026-10-14 — Verification and closure

- Ran `env MPLCONFIGDIR=/tmp/circuitsim-mpl /home/arnold/venv/bin/python tools/rv32i-performance/plot-current-results.py --all`; all four SVGs generated. Visually inspected PNG renders of the predictor, memory and projection charts and parsed the SVG XML. The built copies of all four SVGs match the source bytes.
- `RV32IBenchmarkDocumentationTest` passed 1/1. `/home/arnold/venv/bin/python -m mkdocs build --strict -f mkdocs.yml -d /tmp/circuitsim-docs-results` passed in 38.23 seconds using the existing generated example-row bundle. The rendered §4.7 page links each chart once and contains no `build/experiments/` path. A source-page search found no direct artifact paths, raw result extensions, SHA-256 fingerprint labels or record links; `nettle-sha256` remains as a workload name. `git diff --check` passed.
- Limits: this was a documentation and visualization change. No new simulation, full native suite, WebAssembly package or browser check was run. Full Embench remains a conditional projection from historical cycles.
