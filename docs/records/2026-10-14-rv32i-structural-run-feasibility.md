# RV32I structural comparison feasibility

Started: 2026-10-14. Status: closed. Code state: `github-pages` at `448e4e3`, with pre-existing documentation changes in the worktree.

## Summary

The 35 long branch-pattern cases without structural checks are practical to
run serially on this host. The two full CRC32 cases are technically runnable
with much larger budgets but project to several days of serial structural
simulation. Extending structural comparison to all 133 full Embench cases is
not realistic as a routine campaign here. The reduced-work nine-case Embench
panel already provides complete-program structural evidence at a manageable
scale. New runs on the current source create new results; they do not change
the older artifacts' `structural_checked` fields.

## 2026-10-14 — Evidence and estimates

- The five `20260924-branch64-*` directories contain 35 complete
  compact/oracle/expected-result cases, each with 64 branch iterations and
  `structural_checked=false`. Their historical command set
  `reference_check=false`. The 161 short core-gap cases and three fixed-image
  predictor-capacity cases did run with `--reference-check`.
- The short main-panel branch-pattern structural references used about
  1–2 seconds per case. Scaling each matching pattern/core's recorded
  `host_execution_seconds / cycles` by the corresponding 64-iteration cycle
  count predicts about **6.5 minutes total for 35 reference subprocesses**
  on that older source/host. A measured 64-iteration structural alias case
  took about **16 seconds** for 1,239 cycles, consistent with that order of
  magnitude. Current source and host conditions can change the cost; allow
  additional time for calibration, setup, oracle runs and artifact checks.
  The old reference runs stayed below 100 million events and 512 MiB RSS.
- The reduced-work CRC32 structural references measured **35,907 cycles in
  2,172 seconds** (single), **116,894 in 960 seconds** (baseline), and
  **41,061 in 1,439 seconds** (advanced/two-bit). Full CRC32 behavioral
  summaries report **6,130,443**, **19,966,346** and **7,007,039** cycles
  for those respective targets. Multiplying each structural seconds/cycle by
  the full cycle count projects **103, 45.5 and 68.2 hours** for one full
  structural run of each target. The two September 26 CRC32 pilot cases have
  slightly different cycle counts (20,141,448 baseline and 6,831,935
  advanced); using those exact counts projects **45.9 + 66.5 = 112.4 serial
  hours** for that pair.
  These are rough extrapolations, not measured full structural runtimes.
- The same calculation for full CRC32, SHA-256 and SGLIB across only the
  three middle-panel target configurations projects about **549 serial
  hours (23 days)**. The full Embench matrix has 19 workloads × seven
  targets, so a full structural extension would be much larger. The old
  six-million-instruction CRC32 report also called a full structural run
  impractical on this event-driven simulator.
- The compiled-middle preset caps a reference subprocess at 500,000 cycles,
  20 billion events and 7,200 seconds. Full CRC32 exceeds each relevant
  limit by a large margin; simply adding `--reference-check` to the full
  campaign cannot complete it. The general experiment CLI accepts larger
  explicit budgets, but that does not make the host cost small. The
  measured middle structural runs used under 90 MiB RSS, so observed CPU
  time is the immediate bottleneck, not demonstrated memory exhaustion.

## 2026-10-14 — Decision and verification

- Running the 35 branch cases structurally is feasible and useful. Do one
  current-source case first with the oracle and structural reference,
  certify its clock, and record actual time/events/RSS; then run the 35
  serially if the pilot behaves similarly. The old branch campaigns did not
  freeze their dirty source tree, so a present run is a new source snapshot,
  not a retroactive 199/199 result for September 24.
- For long compiled workloads, retain the behavioral full-suite checks and
  the nine complete reduced-work structural comparisons. Use a bounded
  structural prefix only as diagnostic evidence, never as completed full
  equivalence. A full structural CRC32 or full Embench campaign would need a
  separately budgeted multi-day study and explicit reason to spend that time.
- Inspected the historical `runs.jsonl`, `summary.json`, provenance and
  campaign budgets, plus the current runner options. No new simulation,
  build, CTest or site build was run; all timing estimates depend on the
  recorded older source and host.

Related inventory: [RV32I experiment results audit](2026-10-14-rv32i-experiment-results-audit.md).
