# Realistic timing and cache work: progress log

Started: 2026-10-16. Status: open. Code state: branch `realistic-timing` from
`alpha` at baseline commit `0355289`.

## Summary

Execution log for the [realistic-timing plan](2026-10-16-realistic-timing-plan.md)
followed by the [cache experiment plan](2026-10-14-cache-system-plan.md). The
owner asked for continuous progress without decision requests: each phase
records what was done, which experiments and tests ran, their results, and
any plan changes the assistant made. The public site (`site-docs/`) is
deliberately not updated until the owner allows it; records, status and the
changelog are kept current as usual (owner clarification, 2026-10-16).

Current phase: 3 (realistic gate and wiring delays).

## 2026-10-16 — Phase 0: baseline

- All other sessions' records were closed (memory device retirement, tests
  guide, Pages readiness) and no file had changed for 90 minutes.
- Committed the whole worktree on `alpha` as `0355289` (not pushed). It
  contains the cache component and examples, the two-port arbiter, the
  memory container retirement, the tests guide and the planning records.
  Those sessions' records hold their own verification (focused CTests,
  Pages/WASM builds); no full native suite ran on this exact state, as
  decided (no full suite per phase).
- Created branch `realistic-timing` for all following work.

## 2026-10-16 — Phase 1: timing profiles (in progress)

Design decisions taken while implementing:

- One process-wide active profile (`include/components/TimingProfile.hpp`,
  `src/components/TimingProfile.cpp`), chosen before construction by
  `setActiveTimingProfile`, the Python `set_timing_profile`, or the
  environment variable `CIRCUITSIM_TIMING_PROFILE`; default `unit`. A
  per-`BuildContext` profile was rejected for now: leaf gates are often
  created without a build context, and the analyzer and the behavioral
  parts' gate lists already read each leaf's delay from the built instance,
  so a process-wide choice is enough.
- Leaves reading the profile: the six gates (`Gate.cpp`), `BitSplitter`
  (`BitAdapter.tpp`), the memory `Primitive` base (`DigitalMemory.hpp`) and the
  three SRAM leaves in `SRAM.cpp`. Joiner, rewire, constant and clock stay 0.
  `SRAMAccessControl` stays 0 until phase 4 replaces it.
- The two gate-level loops whose timing is measured rather than path-analysed
  (D flip-flop, SRAM cell) now store their contracts in the profile;
  `DFlipFlopTiming.hpp` and `SRAMCellTiming.hpp` read them, and the two tests
  compare measurement with the active profile. Runtime characterization was
  rejected: the exhaustive search is too slow at picosecond ticks.
- Proven delay tables: one generated file per profile; `unit` keeps
  `ProvenPartDelays.generated.inc`. A profile without a file has no tables.
  Delay-twin and depth-estimate caches are keyed by profile.
  `part_delay_tables --profile=NAME` generates for a profile.
- The build manifest records `timing=NAME` only away from `unit`, so existing
  manifests are unchanged.
- WebAssembly profile selection is deferred to phase 8 (visualizer work).

## 2026-10-16 — Phase 2 folded into the same build

Phase 2 (inertial delay) touches `simulator/Event.hpp`, which forces the same
full rebuild as phase 1, so both were implemented before one build. The first
phase-1-only build was stopped at about 11 %. Implementation:

- `Event::cancelled`; the simulator drops withdrawn events when they reach
  the front of the queue, and `hasPendingEvents`/`nextEventTime` skip them.
- `BasicComponent::inertialDelay()` (false by default; true for the six
  gates). A gate keeps its scheduled-but-unpublished output changes. A new
  change that would land **later** than a pending one withdraws it (and is
  itself dropped if the output already holds that value): a pulse shorter
  than the delay never appears. A new change landing at the **same** tick as
  a pending one keeps transport behavior, so under `unit` (every pending
  change lands at the same tick as the next) nothing can differ.
- `CIRCUITSIM_TRANSPORT_DELAY=1` restores pure transport delay, to tell
  phase-1 and phase-2 effects apart without rebuilding.
- The gate-list delay twin needed no change: it computes when each output
  settles, which inertial filtering does not move.

Verification planned for phases 1–2 (`unit` must be identical): pre-change
binaries saved (`SimulationTestRunner`, `rv32i_experiment` built from the
baseline); compare cycles, ticks and processed-event counts of identical
runs; `part_delay_tables --verify` against the committed table file; the
flip-flop, SRAM-cell and safe-clock tests; a scenario subset.

Phase 3 values prepared (to apply after the phase 1–2 commit):

| Source quantity | Value | Source |
| --- | --- | --- |
| Inverter FO4 delay, 65 nm (`embedded`) | 30 ps | "approximately 30ps at 65nm", arXiv 2606.21454 (COMPOSE) |
| Inverter FO4 delay, 32 nm (`desktop`) | 21 ps | Derived: log-linear between 30 ps at 65 nm and about 10 ps at 7 nm from the same source |
| NAND2, NOR2 in FO4 units | 1.35, 1.55 | Fanout-of-4 gate delays measured across processes, Harris, Ho, Wei, Horowitz, "The Fanout-of-4 Inverter Delay Metric" (1997), figs. 2–4 |
| XOR2 | 4.0 FO4 | Logical effort g = 4, p = 4 (Harris CMOS VLSI lecture catalog), driving four copies: (4·4+4)/5 |
| AND2, OR2 | 1.67, 1.73 FO4 | NAND2/NOR2 driving one inverter (logical effort, h = 1: 0.67/0.73 FO4) plus an FO4 inverter |
| Wiring | 0 | Splitter, joiner, rewire, constant |
| Memory primitives | 1 FO4 | Placeholder until phase 5 gives geometry-dependent values |

Ticks (1 ps): `embedded` NOT 30, NAND 41, NOR 47, AND 50, OR 52, XOR 120;
`desktop` NOT 21, NAND 28, NOR 33, AND 35, OR 36, XOR 84.

## 2026-10-16 — Phases 1–2: build incidents and `unit` identity results

**Incidents.**
- Stopping the first build with `pkill` on `cmake --build` left its gmake
  children running; the restarted build then ran **concurrently** with them
  (duplicated log lines), one test object was written by both and lost its
  `main`. No out-of-memory event occurred. Lesson recorded in the assistant's
  notes: stop a build by its whole process group; the owner warned that an
  out-of-memory event reboots this host, so every heavy job now runs alone
  under `prlimit` with free memory checked first.
- The first identity run found gate-level (reference) CPU runs crashing with
  a segmentation fault during circuit teardown, with or without inertial
  delay. Cause: each gate held `shared_ptr` references to its pending wire
  events, keeping wires alive past their parts. Fixed by holding
  `weak_ptr<Event>` (the project rule: only owners hold `shared_ptr`).

**Checks after the fix (all under `unit`):**

| Check | Result |
| --- | --- |
| `DFlipFlopTimingTest` | passed; setup 6, hold 1, clock-to-output 3–4, high 2, low 6 (= profile) |
| `SRAMCellTimingTest` | passed; 2–3 ticks, spacing 2 (= profile) |
| `SafeClockTimingTest` | passed |
| Nine `rv32i_experiment` runs, baseline binary vs new binary vs new with `CIRCUITSIM_TRANSPORT_DELAY=1`: advanced core programs 19–22 on FF/SRAM/DRAM/NOR, baseline and single-cycle cores, the `stream` workload (21,513 cycles, 1,528,137 events), gate-level advanced core on FF and DRAM (2,008,160 and 2,768,365 events) | Every JSON field identical (cycles, ticks, processed events, oracle checks, stalls) except host seconds |
| Host time, interleaved 3× old/new (load average 8 from other host processes) | `stream` 7.1–8.1 s old vs 7.1–8.0 s new; gate-level program 20 3.5–4.4 s vs 3.8–4.3 s: no measurable change |

`part_delay_tables --verify` is running.
| `part_delay_tables --verify` (8 m 36 s, under 4 GiB) | 601 table entries recomputed from the gates: identical to the stored file; parts without a gate list as stored |
| 23 scenarios (flip-flop, registers, all five endpoints, native-device contract, endpoint timing, cache component and two-level, SRAM cell/device/timing, DRAM, NOR, ROM, register file, full adder, ALU8, Adder32, advanced core and one program) | 23/23 passed |

**Phase 1–2 gate met:** under `unit`, tables, contracts, CPU results and
event counts are identical to the baseline. Committed on `realistic-timing`.
