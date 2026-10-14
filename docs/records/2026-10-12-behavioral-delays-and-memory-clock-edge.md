# Behavioral delays, values after X/Z, and the memory clock-edge rule

Started: 2026-10-12. Status: open. Code state: `github-pages` at `3cfd0fd` before this work; `34abf09` plus a test time-limit change after it. Nothing is pushed.

## Summary

Compact (behavioral) parts now leave X where their gates do, the self-timed
memory sequencer speaks to the transaction port only at clock edges, and the
timing analysis tells a self-timed inside from a clocked interface. Gate-level
and compact systems agree on values and cycle counts in every run compared.

A clock is now certified for a core with a timed memory (period 102 for the
advanced core). The advanced core's clock with the flip-flop memory went from
98 to 100, and the handshake state lists are checked against simulation (see
the last two sections).

The selected FF and timed-memory performance panels were subsequently rerun
at the certified periods; the current coverage and the separate full Embench
projection are in [§4.7](../../site-docs/rv32i/experiments.md). Still open:
the compact five-stage core remains coarse about X, 24 delay-rule exceptions
are recorded, and configurations outside the measured panels need separate
checks. The dated rerun requirements below describe the state before those
later campaigns.

## 2026-10-12 — Changes

- `c93be7c`: compact parts follow the gates' four-state behaviour through
  shared rule functions (`registerStep`, `rv32i::register_file::step`,
  `incrementBy4`, the memory handshake helpers). Value differences after X/Z
  fell from 56,096 to 16,396; none remain where only the compact form shows a
  0 or 1. The rest are the compact five-stage core showing X where the gates
  still know the value. Experiments report `setup_violations`.
- `42d11d3`: a first port-level fix for the memory handshake; replaced by the
  next commit. The register file went back to its delay table, because its
  gate list made gate-level runs 21% slower with no gain.
- `41cdf18`: `NativeSequence` shows DONE, result, status and readiness only at
  a clock edge; `MemoryTransactionPort` is back to its earlier form. This
  removes lost responses and hangs seen at SRAM periods 108/110/112 and DRAM
  98/100/102. Analysis: `clocked_pins`, `self_timed_escapes`,
  `clocked_interface` (270 endpoint tables). `--period auto` refuses non-FF
  memories with a clear message.
- `34abf09`: `DelayRuleTest` records 16 self-timed controllers and endpoints
  as exceptions (reason `LAUNCH`, 57-67%, no rule break).

## 2026-10-13 — Verification

- Advanced core, program 20, periods 100/108/120/166: SRAM and ROM
  400/334/334/268 cycles, NOR-flash 334/334/334/268, DRAM 588/564/537/442.
  Gate-level equals compact in all 14 runs compared. No setup violation from
  period 100 up; 51 at period 98.
- `DelayRuleTest` on `34abf09`: 138 parts, 120,081 situations, 0 rule breaks,
  95% typical, 24 recorded exceptions.
- Full native suite on `34abf09`, one test at a time: 450 of 451 passed in
  5,772 s. `RV32IBinaryClockCLITest` hit its 180 s limit; run alone it passed
  in 134 s. Its limit was raised to 300 s. Whether it became slower through
  this work was not measured.
- `tools/build-pages.sh` succeeded (layout validation, example rows, strict
  MkDocs). Web checks: 36 of 36 (14 parity, 22 browser).
- Not run: the published-result reruns for sequenced memories.

## 2026-10-13 — Decisions and remaining work

- The compact cores stay whole-core models. Building them from compact
  children gave the same cycle counts but ran 14-136 times slower.
- The handshake state model (`src/analysis/MemoryTimingState.cpp`) covers only
  the flip-flop endpoint, so analysis of a core with a sequenced memory stops
  at "Combinational dependency cycle". Extending it is larger than first
  estimated and was not started.

## 2026-10-13 — Clock certified for timed memories

Replaces "Extending it ... was not started" in the section above.

- `sequencedStates` (`src/analysis/MemoryTimingState.cpp`) enumerates the
  reachable handshake states of a timed endpoint with the core's requester:
  30 states for SRAM, ROM and NOR-flash, 42 for DRAM, merged into 6 and 8
  timing cases by what the endpoint shows. The engine settles them with a new
  `Sequenced` part mode. Result for the gates of the advanced core: period 92
  with each of the four memories.
- At 92 a compact run reported 51 setup violations: the compact core's table
  asks for `IMEM_REQ_READY` 19 ticks before the edge, the gates need less in
  the states where it changes late. The experiment now adds the compact core's
  setup needs at its memory pins to the analysis
  (`ClockSpecification::also_captured`), for timed memories only. `--period
  auto` gives 102. The flip-flop memory stays at 98 and is not changed.
- Checked at `--period auto`, advanced core, program 20, gates and compact:
  SRAM 400 and 400 cycles, ROM 400 and 400, NOR-flash 334 and 334, DRAM 583
  and 583; all verified, 0 setup violations. Policies `taken` and `two-bit`
  with SRAM also certify 102. The analysis takes about 2 s (SRAM) and 3 s (DRAM).
- Tests run and passed: `SafeClockTimingTest`, `MemoryEndpointSafeClockTest`,
  `MemoryStateTimingTest`, `MemoryFeedbackTimingTest`, `BinaryStartupTimingTest`,
  `SettlingBoundTest`, `PartDelayTablesTest`, `RV32IExperimentCLITest` (updated
  to expect the certificate), `RV32IBinaryClockCLITest`, `RV32IBenchmarkCLITest`,
  `FullEmbenchCampaignTest` and two campaign tests. The full suite and the site
  build were not rerun after this change.
- Limits: no dynamic test yet compares states seen in a simulation with the
  enumerated ones; no binary-startup certificate for timed memories; the
  analysis uses the endpoint's delay table, not its gates.

## 2026-10-13 — Flip-flop clock includes the compact core; state lists checked

Replaces "The flip-flop memory stays at 98 and is not changed" and "no dynamic
test yet compares states" in the section above.

- The compact core's setup needs at its memory pins are now added for every
  memory. Certified periods: single-cycle 166 and baseline 80 (unchanged),
  advanced 100 with the flip-flop memory (was 98), 102 with a timed memory.
  Published advanced-core results at period 98 are therefore out of date;
  cycle counts are not expected to change, times scale with the period. Not
  rerun.
- `rv32i_experiment --fidelity reference --check-handshake-states` fails if a
  memory port is, in any cycle, in a state outside the list the analysis used.
  Program 20 at `--period auto`: flip-flop 170 port states, SRAM 800, DRAM
  1166, ROM 800, NOR-flash 668; all covered, all runs verified, 0 setup
  violations. `RV32IExperimentCLITest` runs this for flip-flop, SRAM and DRAM.
- Tests run and passed: `SafeClockTimingTest`, `MemoryEndpointSafeClockTest`,
  `MemoryStateTimingTest`, `MemoryFeedbackTimingTest`, `BenchmarkCampaignCLITest`,
  `FullEmbenchCampaignTest`, `RV32IBenchmarkCLITest`, `RV32IExperimentCLITest`,
  `RV32IBinaryClockCLITest`. By request the full suite and the site build were
  not rerun after `34abf09`.

## 2026-10-13 — Clean rebuild and full native suite at `448e4e3`

Replaces "the full suite ... not rerun after `34abf09`" in the section above
for the native suite only.

- `cmake --build build-cleanup -j1 --clean-first` (4 GB address-space cap):
  finished with exit code 0 and no compiler errors.
- `ctest --test-dir build-cleanup -j1`: 451 of 451 tests passed, 2404 s.
- Not run: the site build (`tools/build-pages.sh`) and the browser checks
  (`tools/run-web-tests.py`).
