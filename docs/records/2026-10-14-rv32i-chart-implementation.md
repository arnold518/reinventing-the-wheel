# RV32I results chart implementation

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation and other worktree changes.

## Summary

The approved [chart reset plan](2026-10-14-rv32i-chart-reset.md) is implemented in public §4.7. Four reproducible figures show all seven FF targets in every core comparison, with full Embench explicitly marked as a historical-cycle projection. All plotted values were checked against complete campaign evidence and the adjacent public tables; no benchmark was rerun.

## 2026-10-14 — Input and scope check

- The current public §4.7 page contains complete modeled-tick tables for five seven-target FF groups. The fixed group has eight expected-trap programs outside its 14-case performance summary. The generated and core-gap groups overlap; their summaries must remain separate. The long-branch group is five 64-iteration patterns, and two checked-in ELF fixtures form the fifth group.
- Complete current-source local summaries exist for fixed (154 rows), generated (105), core-gap (161), long branch (five times seven), and checked-in ELF fixtures (two times seven). Their FF periods are 166/80/100 ticks for single/baseline/advanced. The 19-workload × seven-target full Embench input is a clock-only projection from historical cycles and requires its own explicit status.
- The repository has a large pre-existing dirty worktree. This change is limited to the §4.7 charts/page, a reproducible chart generator, this topic record, status, and changelog.

## 2026-10-14 — Figures and data checks

- Added `tools/rv32i-performance/plot-current-results.py` and four SVGs under `site-docs/rv32i/4.7/`: a five-group measured FF overview, two controlled mechanism contrasts, a five-pattern seven-target longer-branch matrix, and a separately labeled 19-workload seven-target conditional Embench projection. Every core comparison displays single-cycle, baseline, and all five advanced policies. Single-cycle is visible at 1.0 where it is a denominator.
- The script reads completed current-source fixed, generated, core-gap, branch64 and checked-in ELF campaigns. It requires the shared source SHA-256 `4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2` and binary SHA-256 `e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1`, complete seven-target grids, certified FF periods 166/80/100, completed oracle/reference/measurement phases, zero setup violations, and summary/measurement/final-state agreement. It verifies `ticks = cycles × period`.
- The projection path checks all 133 projected CSV rows against the completed September 27 full Embench case summaries and oracle checks at historical periods 116/62/78, then checks their multiplication by current periods 166/80/100. This is still a projection: the script cannot prove unchanged cycles on the current source, and those full runs lack structural-reference checks.
- Before rendering, the script compares 602 displayed modeled-tick cells across the public fixed, generated, core-gap, longer-branch, fixture and projected Embench tables with those inputs. It completed this validation and wrote four SVGs. PNG previews were inspected: all seven targets and labels are visible, the longer-branch winners are outlined, and the projection heading and caveat are legible. Timed-memory, predictor-capacity and expected-trap results remain in tables under the reset plan.

## 2026-10-14 — Verification and closure

- Ran `MPLCONFIGDIR=/tmp/circuitsim-mpl /home/arnold/venv/bin/python tools/rv32i-performance/plot-current-results.py --preview-dir /tmp/rv32i-chart-previews`; it validated 602 public cells and generated four SVGs. The generated SVGs parse as XML and each has a title. The built site's four SVG copies are byte-identical to the source assets, and the rendered §4.7 page links each exactly once. Public §4.7 contains no experiment artifact paths, source/binary hashes or direct raw-result links.
- `RV32IBenchmarkDocumentationTest` and `RV32IReferenceDocumentationTest` passed 2/2. The strict MkDocs build passed in 31.59 seconds using the existing generated example-row bundle. `git diff --check` passed. No full native suite, WebAssembly package, browser check or new simulation run was performed for these static figures.
- The displayed FF group statistics are unweighted within groups, with overlapping generated/core-gap definitions kept separate. The 19-workload Embench heatmap is a conditional projection and cannot establish current-source cycles or performance. The timed-memory panel remains advanced-only; it was not turned into a three-core figure.
- Two consecutive regenerations with the same Matplotlib 3.10.3 environment produced byte-identical SVGs. Matplotlib 3.10.6 changed SVG bytes on this host, so the generator's data validation is stable across those versions but byte-for-byte asset comparison requires the same plotting version. The final source SVGs match the strict-build copies.
