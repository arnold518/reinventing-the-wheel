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

Current phase: 8 (phases 1–7 done). Phase 6 gave datasheet DRAM and NOR
timing, a clocked on-chip SRAM macro, endpoints that take requests from
registered state only, one DRAM synchronizer edge per command, and a 306-run
core sweep. Phase 7 gave the direct-bank cache (two-cycle hit), burst reads
and burst line fills, and the cache experiment (Step 1, standalone traces,
locality traces, five-topology CPU panel; exploratory, unpublished). Phase 8
so far: the clocked macro's write disturb fixed; DRAM command counters; the
native suite kept as a `unit` suite with a realistic-profile sweep; the
delay twin's X timing fixed; visualizer data refreshed; cached and shared
roots certified by the safe-clock analyzer at the core's clock. Open: the
public site (`site-docs/`, waiting for the owner) and the documentation
checks that read it; the STM32F7 check; reruns of published results.

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

## 2026-10-16 — Phase 3: realistic gate delays (in progress)

Done so far:

- Added the `embedded` (65 nm, FO4 30 ps) and `desktop` (32 nm, FO4 21 ps)
  profiles with the gate values recorded above; 1 tick = 1 ps; wiring 0;
  memory primitives one FO4 as a placeholder.
- `DFlipFlopTimingTest` and `SRAMCellTimingTest` now measure every profile,
  with search windows scaled to the slowest gate involved, and step from
  event to event instead of tick by tick (same results under `unit`; a
  tick-by-tick search would take hours at picosecond ticks). The SRAM-cell
  overlap search is bounded by four settling times (24 ticks under `unit`,
  unchanged).
- Measured and stored contracts:

  | Profile | Flip-flop setup / hold / clock-to-output / high / low | SRAM cell earliest / latest / spacing | Runtime |
  | --- | --- | --- | --- |
  | `unit` | 6 / 1 / 3–4 / 2 / 6 | 2 / 3 / 2 | — |
  | `embedded` | 231 / 0 / 132–175 / 52 / 231 ps | 71 / 112 / 42 ps | flip-flop 20 s all profiles; cell 3 m 42 s |
  | `desktop` | 160 / 0 / 91–120 / 36 / 160 ps | 49 / 77 / 29 ps | |

  Both tests pass for all three profiles.
- Generating `ProvenPartDelays.embedded.generated.inc` and
  `ProvenPartDelays.desktop.generated.inc`.

**Plan change (phase 3 acceptance).** The consolidated plan required every
scenario and program test to pass under the realistic profiles. The scenario
tests hard-code clock phases in ticks (for example 200-tick half periods),
which are far too short at 1 ps per tick; scaling every scenario would mean
rewriting hundreds of tests and contradicts decision 10 (existing tests stay
on `unit`). Phase 3 under realistic profiles is instead accepted on: the
per-profile contract tests; per-profile delay tables verified against the
gates; CPU runs at each profile's certified clock with the instruction
oracle, gate-level against compact; and a `DelayRuleTest` sample. Timed
memories (SRAM/DRAM/ROM/NOR) are excluded under realistic profiles until
phases 4–6 replace their tick-based timers: their 32-tick phases would mean
32 ps.

## 2026-10-16 — Phase 3: table generation needs a fixed point; memory incident

- The first `--write` for the realistic profiles omitted clocked parts (197
  tables); the `unit` file is generated with `--clocked`. Rerun with
  `--clocked`: 600 tables each (embedded 557 s, desktop 658 s, peak about
  350 MB). The one missing key, `rv32i.system.educational-single-cycle`, failed
  for "no timing contract for FFMemory32".
- Cause, and a correction to the generation procedure: the generator analyses
  each part with its storage and memory children in compact form, and those
  children use the tables **compiled into the generator**. The binary had no
  realistic tables yet, so children fell back to their single 1-tick delay;
  the first-pass realistic tables are therefore wrong for every part with
  compact children. Procedure adopted: compile the tables in, regenerate,
  repeat until `--verify` reports the compiled-in tables identical to a fresh
  generation (a fixed point).
- **Memory incident.** Compiling `ProvenPartDelays.cpp` with all three
  generated files in one source drove the host into heavy swapping (load
  average 192) until the build was terminated; there was no reboot or OOM
  kill. Measured afterwards: one table file alone needs 1.1 GB peak and 74 s
  at `-O3`. Fixed by one source file per profile
  (`ProvenPartDelaysUnit/Embedded/Desktop.cpp`); the library then built in
  4 m 12 s under a 3.5 GiB cap.

## 2026-10-16 — Phase 3 results and acceptance

**Tables.** Second generation pass, with first-pass tables compiled in:
601 tables per profile (the same keys as `unit`); 62 entries changed from
pass 1, all parts with compact storage children (pipeline registers,
stages, fetch/decode, cores). Rebuilt; `part_delay_tables --verify` then
reported both files **identical** to a fresh generation (fixed point; 285 s
each, peak 348–376 MB). CTest entries `PartDelayTablesComplete_embedded_Test`
and `..._desktop_Test` added (label `slow`).

**Certified clocks** (`rv32i_experiment --safe-clock`, FF memory, all
`safe-under-contracts`; the limiting path is the same in every profile):

| Core | `unit` ticks | `embedded` ps (MHz) | `desktop` ps (MHz) | Limiting path |
| --- | ---: | ---: | ---: | --- |
| Single-cycle | 166 | 7,164 (140) | 4,986 (201) | register-file write data |
| Baseline pipeline | 80 | 3,354 (298) | 2,334 (428) | IF/ID instruction register |
| Advanced pipeline | 100 | 4,508 (222) | 3,140 (318) | instruction-memory REQ_READY |

The `embedded` frequencies are in the range of real 65 nm microcontrollers
(the STM32F7 runs at 216 MHz), a plausibility check only.

**Programs at the certified periods** (FF memory): programs 19–22, `stream`,
`sort` on the advanced core; program 20 on all three cores in compact and
gate-level form. Under `unit`, `embedded` and `desktop`: identical cycle
counts (for example `sort` 94,345, `stream` 21,513), zero setup violations,
instruction oracle and final state verified in every run.

**Cost.** Gate-level runs processed 15–40 % fewer events under realistic
profiles (advanced 2,008,160 → 1,705,333; single-cycle 2,449,783 →
1,486,915), from swallowed pulses and zero-delay wiring; host time equal or
lower.

**Margin.** The gate-level advanced core under `embedded` stays correct at
4,400, 4,000 and 3,600 ps (80 % of the certificate) and fails at 3,000 ps:
the certificate is a safe bound, not the exact limit.

**DelayRuleTest sample** (`--report` on the 32-bit adder, ALU8, 32:1 mux,
32-bit register, decode control, forwarding, FF endpoint): passed under `unit`
and `embedded`; no compact part faster than its gates, none outside the
closeness targets.

**Phase 3 gate met** (as revised: contract tests, verified per-profile tables,
oracle-checked compact and gate-level CPU runs at certified clocks, delay-rule
sample). Timed memories remain excluded under realistic profiles until
phases 4–6.

## 2026-10-16 — Plan check after phase 3

- On track. The profile mechanism, inertial delay and realistic gate delays
  work without changing `unit`. The measured CPU clocks are plausible.
- New procedure rule for every later structural change: regenerate realistic
  tables to a fixed point (generate, rebuild, `--verify`).
- Tag `timing-legacy` is placed on the phase 3 commit: it reproduces every
  published result under `unit`, before phase 4 changes memory timing.
- Phase 4 starts next (gate-level memory control).

## 2026-10-16 — Phase 4 design (assistant decisions)

**Plan change: phases 4 and 5 merge.** Decision 2A makes every device time
itself and raise a completion signal. Building gate-level control first on
temporary tick timers and then replacing them with replica timing would do
the device control twice, so each technology's control logic and its
self-timing are built together. Phase numbering of the consolidated plan is
otherwise unchanged (calibration stays phase 6).

**New leaf parts.**
- `MemoryDelayLine`: an analog interval (bitline precharge, sense
  development, DRAM restore, flash program and erase pulses) as one leaf whose
  delay comes from the timing profile and the device geometry. Inertial, so
  a restarted access withdraws a pending edge. Long intervals are single
  events, not gate chains (decision 3).
- `Synchronizer`: two clocked stages capturing an asynchronous device signal;
  a stage resolves an input that is unknown or changing at the edge to its
  previous value instead of X (metastability resolution is analog and not
  modelled). The timing analyzer treats its data input as an asynchronous
  capture (decision 7).

**Controllers (gates), plugged into the existing gate-level transaction port
in place of `NativeSequencer`.**
- Static (SRAM, ROM): request registers loaded on START; one-hot states
  ACCESS → DONE; CE/WE/OE and address/data driven from the registers; the
  device's READY passes through the synchronizer; on synchronized READY the
  result and status registers load and CE is released. `BACK_READY` requires
  idle and synchronized READY low, which flushes a stale READY before the next
  access. Invalid requests complete at once with the gate-level request check's
  status (generalized from `CacheRequestCheck`/`FFRequestCheck` with per-
  technology permissions).
- Command (DRAM, NOR): device CLK is the system clock; a command is presented
  for one ISSUE cycle and the device latches it at the next edge. Each WAIT
  skips two edges (synchronizer flush, since BUSY is known high from the
  accepting edge) and then waits for synchronized BUSY low; ERROR, READ_VALID
  and READ_DATA are stable by then and read directly. DRAM: open-row bit and
  row register with a comparator choose PRECHARGE/ACTIVATE; a cycle counter
  (`refresh_interval_cycles`) raises refresh-pending, served from idle while
  new requests wait. NOR: WREN before PROGRAM/ERASE.

**Devices (gates plus stand-in leaves).**
- SRAM: address/control change detection (comparators against latched inputs)
  starts a precharge interval; the wordline enables after it; a replica
  bitline (the same bitline primitive and row count as the data columns) fires
  sense enable; READY follows the sense amplifier.
- ROM: change detection, decode, a replica read column; READY.
- DRAM: command latch and decoder, legality checks, BUSY/ERROR/ROW_OPEN state
  bits, refresh-row counter; ACTIVATE waits for a replica sense column, then
  a restore interval; READ/WRITE and PRECHARGE use intervals; REFRESH is
  ACTIVATE plus PRECHARGE on the counter row.
- NOR: write-enable latch, command decoder, block-erase row decoder in gates;
  READ via replica read column; PROGRAM/ERASE intervals.

**Interval values.** Held in the timing profile per technology. `unit` keeps
values close to today's phase multiples of 32 ticks so `unit` memory timing
stays of similar size (exact equality is not required after the
`timing-legacy` tag); realistic values are placeholders until phase 6.

**Behavioral twins.** `NativeSequence` is rewritten as a cycle-exact model of
the gate controllers; device models and compact devices follow the new
self-timing. Equivalence: identical cycles and responses; timing within a
cycle from regenerated tables.

**Order.** 4.1 leaves and analyzer contracts → 4.2 SRAM/ROM devices and the
static controller with twins and tests → 4.3 DRAM/NOR devices and the command
controller with twins and tests → 4.4 core certification model
(`MemoryTimingState`), tables to a fixed point per profile, CPU runs with timed
memories. A commit after each step.

## 2026-10-16 — Phase 4.1–4.2: timing leaves, SRAM/ROM control as gates

**New leaves** (`MemoryTimingParts`): `MemoryDelayLine` (analog interval;
inertial; optional quick fall, used reset-dominant in timer stages),
`SymbolicDiffer` (the XOR of a transition detector, reading X as a symbol so a
steady unknown input is steady; XOR delay), `Synchronizer` (two stages,
resolves an unknown or changing sample to the previous value; asynchronous
capture in the analyzer), `ProtocolStableData` (a visible multicycle-path
exception: the controller takes read data only after READY has passed the
synchronizer). `MemoryTimer` is the gate-level self-timing block: each input
held through a settle line, transition detection, an OR tree, and stages of
delay lines and replica AND chains, every stage gated by "started".

**SRAM.** `SRAMAccessControl` is now gates around a `MemoryTimer`: address,
CE_N and WE_N always restart an access; data and mask only while writing (a
"conditional" timer input). Precharge for one interval; row enable after a
replica covering both the row decoder and the release of precharge at the
bitlines; sense enable and the new READY output after a replica bitline
interval. The bitline pair's delay is the same profile value
(`sramBitline(rows)`). Unit intervals: settle 32, precharge 32, bitline 64
(a full access is 140 ticks, close to the former 4 × 32 + guards).

**ROM.** `ROMAddressControl` is now gates: held address split into row and
column fields, a gate range check (`addressBelow`, shared helper), enables,
and READY from a replica decoder plus a replica read column (the read
column's delay is `romColumn(rows)`).

**Static controller.** `StaticMemorySequencer` (gates) replaces the C++
sequencer for SRAM and ROM: gate-level request check with the same status
precedence (`MemoryRequestCheck`), request registers, ACCESS/DONE/IS_WRITE
bits, CE/WE/OE from the registers, READY through the synchronizer, result
and status registers. The behavioral controller and the fused endpoint run a
cycle-exact copy (`NativeSequence::stepStatic`); the fused endpoint's SRAM
and ROM models time READY from the device and controller tables.

**Compact devices.** SRAM and ROM compact devices time READY and READ_DATA
from the table proven on the gates, apply each output change at its own time
(a pin set early would show the result an access too soon in snapshots), drop
READY at the latest of the changed pins' minimum delays (never before the
gates), and declare themselves self-timed. The SRAM table is proven per WE_N.

**Problems found and fixed during bring-up**
- a late rising edge of a stage delay line after a restart (false READY):
  timer stages now fall within one gate;
- the row could open while precharge was still asserted at the bitlines for
  small arrays (replica too short): the replica covers the precharge release;
- XOR of a steady unknown input gave X forever: `SymbolicDiffer`;
- compact outputs set on their pins at schedule time looked early in
  snapshots: timed outputs applied at their times;
- a duplicate part name (`BAD_MASK`) crashed construction: constants renamed;
- certification of cores with SRAM lost its handshake model and found a
  combinational loop plus a 235-tick internal path: the read data path is now
  a `ProtocolStableData` exception and the state model accepts a clocked
  interface and the new "released, not yet ready" state.

**Known pessimism (recorded).** The SRAM device's longest data arc (215 at
unit) runs through a write and back through a cell, which no access takes;
the SRAM cell contract lets every input reach Q even with its write drive
off. The compact device follows the table, so its data appear after its
gates' (allowed); cycles are unaffected because the controller samples data
two or more edges after READY. A state-dependent cell contract would remove
it.

**Test changes.** `ROMAddressControlTest`: an out-of-range address now leaves
the output enabled (the data are X) and an unknown address gives an unknown
row enable. `DelayRuleTest`: a compact part that declares itself self-timed is
compared with its gates only.

**Checks (unit).** 19 scenarios passed: SRAM device/contract/timing/access
control, ROM device and address control, native-device contract, all five
endpoints, endpoint contract and timing, the four cache scenarios and
`CacheReadTest`. `DelayRuleTest --report` on SRAM/ROM devices, controllers and
endpoints (8/16/32): no rule break; the controllers now are ordinary clocked
parts (their old exceptions kept, floors below measured values).
`rv32i_experiment` (advanced, oracle verified): program 21 on SRAM 696 cycles
(was 558), program 22 on SRAM 487 (was 420), program 19 on ROM 472 (was 470);
safe clock with SRAM and ROM again certified at 102 ticks (64 state cases).
The cycle increase comes from the two-stage synchronizer and the release
before the next access. Realistic-profile tables for these parts are stale
until regenerated at the end of phase 4.

## 2026-10-16 — Phase 4.3: DRAM and NOR control as gates (in progress)

**Decision change (replaces the command-controller part of "Phase 4
design").** The design above had the command controller synchronize the
device's BUSY through a two-stage synchronizer and skip two edges per wait.
Since the device is clocked by the controller's own clock (MEM_CLK is the
system clock through a zero-delay bridge), its BUSY, ERROR, READ_VALID and
ROW_OPEN are registered in the same clock domain, so no synchronizer is
needed. The asynchronous part is inside the device: its analog command chain
(delay lines) ends in a one-stage sampler, and BUSY is released one edge after
the sampled completion. A command therefore costs: ISSUE cycle, device takes
it at the next edge, three device edges (sampled, complete, released), and the
controller sees BUSY low at the following edge, about five edges in all.

**Devices.** `DRAMCommandControl` and `FlashControl` are gates: power-on
reset, command decode and legality, per-command delay-line chains, the
sampler, a releasing state that keeps BUSY high one more edge, the refresh row
counter (DRAM) and the write-enable latch and block-erase row decoder (NOR).
The compact devices and the fused endpoint models run `DRAMSyncControl` and
`NORSyncControl`, cycle-for-cycle copies with the gates' step times.

**Controller.** `CommandMemorySequencer` (gates) replaces the `NativeSequencer`
primitive (removed): gate-level request check, request registers, a pending
step list chosen by priority (DRAM: PRECHARGE, ACTIVATE, READ/WRITE, REFRESH;
NOR: WREN, then READ/PROGRAM/ERASE), ISSUE and WAIT bits, the command
register shown for one cycle, open-row knowledge (ROW_KNOWN, OPEN_ROW register
and comparator together with the device's ROW_OPEN), a refresh cycle counter
(`refresh_interval_cycles`, default 96) with pending and refreshing bits, and
result and status registers. A failed command (device ERROR, or a read without
READ_VALID) ends the request with status Device. `NativeSequence` was rewritten
as a cycle-exact copy for the compact controller and the fused endpoint.

**Problems found and fixed during bring-up**
- one-bit bit adapters and unsupported register widths in the refresh
  counters: widths are rounded to ones that have registers and bit adapters;
- the compact DRAM woke at the last chain step but not at the chain end
  reaching the sampler (the OR tree), so at the next edge in a settled test
  the gates had sampled and the compact part had not: it now wakes at the
  chain end too;
- on the starting edge the operation-kind bits still held the previous
  request's kind, so a read after a write was issued as a WRITE (and on NOR a
  read after a program as PROGRAM): the kind now comes from the new request
  on that edge (gates and copy);
- the fused endpoint let the sequencer read device outputs updated at the
  same edge; it now reads the device's outputs from before the edge for the
  clocked devices (static devices unchanged);
- a refresh interval shorter than one refresh's service starved requests:
  `refresh_interval_cycles` must be 0 or at least 24 (validated).

**Behavior changes recorded.** A protocol reset now drops a request whose
device commands have not been issued; a command already issued completes
(the device has no reset). Refresh is counted in clock cycles, so the
configuration check against `retention_ticks` (still in ticks) cannot account
for the clock period; it is a configuration responsibility for now.

**Test changes.** Device scenarios give each command three more clock edges
(the device is synchronous); `MemoryDeviceTimingTest` drives a free-running
clock. Endpoint protocol scenarios wait up to 20 cycles and disable DRAM
refresh (refresh is covered by `MemoryEndpointTimingTest`, now at
`refresh_interval_cycles=24`, retention 25000). The flash reset check resets
after the PROGRAM has reached the device. The inspector check expects the
new phase names `command-issue` and `command-wait`.

**Checks (unit), all passed:** DRAMCommandControlTest, DRAMTest,
NORFlashCommandControlTest, NORFlashTest, MemoryDeviceTimingTest,
MemoryDeviceContractTest (includes the DRAM and NOR byte-mask oracles),
NativeMemoryDeviceTest, NativeMemoryDeviceContractTest, MemoryControllerTest,
MemoryEndpointFF/SRAM/DRAM/ROM/FlashTest, ScalableMemoryEndpointTest,
MemoryEndpointContractTest, MemoryEndpointTimingTest, MemoryHelperContractTest.
Each component scenario runs every fidelity and compares them, so the gate
controller and its copy agree on every checkpoint.

**Tables (unit).** `part_delay_tables --clocked` regenerated 177 DRAM and NOR
tables (device, controller, endpoint, scalable endpoint, all widths and
geometry variants) and they were merged into the unit file. The devices'
outputs are all registered (0 combinational pairs; BUSY/ERROR/READ_VALID/
ROW_OPEN launch 3–4 ticks after the clock; DRAM READ_DATA up to 177, taken
only after READ_VALID through `ProtocolStableData`). The endpoints are clocked
interfaces (REQ_ADDR setup 35, RSP_READY setup 28 at unit).

**DelayRuleTest (unit)** on DRAM and NOR devices, controllers and endpoints
(8/16/32): 0 rule breaks over 17,063 situations; every compact output change
at its table delay, never before the gates. The DRAM controllers answer at
their table's latest launch after an edge, a few ticks after the gates (worst
share 64–73 %), the same pattern as the NOR, SRAM and ROM controllers; floors
were recorded with the LAUNCH reason.

**CPU runs (unit, advanced core, two-bit predictor, oracle-verified).** Safe
clock: DRAM 106 ticks, NOR 104 (all timed memories certified at 102 before;
the limit is the instruction endpoint's RSP_READY setup).

| Program | DRAM before (102) | DRAM now (106) | NOR before (102) | NOR now (104) |
| --- | ---: | ---: | ---: | ---: |
| 19 | 648 | 1,086 | 403 | 604 |
| 20 | 507 | 868 | 274 | 436 |
| 21 | 807 | 1,375 | 471 | 732 |
| 22 | 655 | 1,108 | — | — |

All runs completed and verified. The cycle growth (about 1.7× DRAM, 1.5×
NOR) comes from the clocked command protocol: each command costs about five
edges (issue, device takes it, sampled, complete, released, controller sees
BUSY low), where the former C++ sequencer polled within a cycle in ticks. Per-request
latencies were not measured separately in this step.

**Plan check after 4.3.** On track. The per-command edge count is a property
of this device design (one-stage sampler plus a release edge for
make-before-break safety), not of physical timing; phase 6 calibrates the
analog intervals, and the edge overhead is accepted as the model's protocol
cost. For the cache experiment this makes main memory slower in cycles,
which favours caches; the experiment will report it as such rather than
tuning it away. Next: 4.4 (tables to a fixed point for the realistic
profiles, CPU runs under all profiles), then commit.

**Full unit table check.** `part_delay_tables --verify` on the whole unit
file first reported differences: the twelve storage-bank tables (SRAM, ROM,
DRAM, NOR at 8/16/32) were stale since 4.2/4.3, because the bank primitives
now carry their geometry delays (SRAM bitline 64–131, ROM column 96, NOR read
97, DRAM sense 32 at unit) while the stored tables still had 1–5; and
`NoGateList.generated.inc` still described the SRAM/ROM controllers as
"stored state" instead of self-timed. Both were regenerated; the re-verify
after the rebuild is identical (601 entries, gate list as stored). The
phase 4.2 check had regenerated only the device, controller and endpoint
tables, which is how the banks were missed: from now on the whole file is
verified at the end of each step.

**Python checks.** `MemoryInspectorTest` and `MemoryInspectorIntegrationTest`
pass. `MemoryEndpointIntegrationTest`: the device pin-set check was updated
for the READY output that SRAM and ROM gained in 4.2 (missed then); two
example checks still fail because they compile reference models from
`site-docs/memory` code snippets (`memory_example::CompactDevice`,
`Sequence`) that describe the old tick sequencing. They are rewritten with
the public pages in phase 8 (site-docs untouched by owner request).

**Final focused run on the committed state (unit), 31 passed:** SRAM cell,
device and access control; ROM device, address control and bank; DRAM and NOR
banks, command controls and devices; device contract and timing; native
device and contract; controller; all five endpoints, scalable endpoint,
endpoint contract and timing; helper contract; `CacheReadTest` and the four
cache scenarios.

## 2026-10-16 — Phase 4.4: realistic profiles (in progress)

Phase 4.3 was committed as `7bdb4fd`. Step 4.4 regenerated the `embedded`
and `desktop` tables (only memory parts changed, 429 entries each; fixed point
in one step) and ran the timed-memory CPU panel and the memory scenarios
under all three profiles.

**First panel (before the fixes below).** All `unit` runs and all FF, SRAM,
ROM and NOR runs under `embedded`/`desktop` completed and verified. Two DRAM
runs (embedded program 20, desktop program 22) stopped with "Cannot read
unknown memory byte": the experiment's final memory readout landed while a
row was open. The DRAM models wrote X into storage while a row was open
(the cells' destructive read) until sensing restored it, and with realistic
sensing of a 32,768-row array (the placeholder's per-row term, about 100 ns)
that window is long. Since sensing always restores the row before the
wordline closes, the compact and fused DRAM models now keep storage as the
row's logical contents and carry the open row only in the row buffer (an
expired row still becomes X when it is restored). Both runs then verified;
DRAM tests passed again under `unit`.

**Memory scenarios under the realistic profiles** found two real design
faults in the gate-level devices (the `unit` profile hid both because every
gate takes one tick):

1. *SRAM change detection lost its pulse.* The timer sees an input change as
   the XOR of the input and its settle-delayed copy, a pulse as long as the
   settle line. The placeholder settle (3 FO4, 90 ps) was shorter than the
   XOR (120 ps), so inertial delay swallowed the pulse and a read after a
   write never restarted the access (probe: READY and ROW_ENABLE stayed high,
   READ_DATA X). The held inputs must also not change before the pulse has
   reset the stages. Fix: `memoryTimerSettle` sizes the settle line to at
   least the detection path (XOR, condition AND, OR tree, STEADY, STARTED,
   stage reset gate), as an address-transition detector is sized against its
   own logic; `unit` is unchanged (path 11–12 ticks < 32).
2. *DRAM sensing raced the wordline.* The wordline reaches the cells through
   the row decoder while SENSE and PRECHARGE go to the bank directly. With
   three rows the decoder is three ANDs (150 ps), which tied the sense
   step, so a reopened row was sensed before its samples arrived and read X
   (16-bit oracle and endpoint scenarios). Fix: the sense step and the
   precharge-after-close step carry a decoder replica
   (`dramDecodeReplica`: enable AND, AND tree, one gate margin) and the
   sense step also the cell's delay; the cycle-exact copy follows. This
   changes DRAM timing under `unit` slightly.

**Observation kept for phase 5/7.** SRAM READY rises at the last timer stage,
before the sense amplifier and output buffer have produced the data (about
210 ps at `embedded`, a few ticks at `unit`). The controller's synchronizer
and `ProtocolStableData` cover it, but a cache bank driven directly
(phase 7) needs READY after data; the READY replica must then include the
sense and output path.

**Tests that drive fixed tick times** (`MemoryDeviceTimingTest`,
`MemoryEndpointTimingTest`, `CacheReadTest` with 100-tick half-cycles) fail
under realistic profiles as expected; they stay `unit` tests under the phase
3 rule until their periods are derived from the profile (phase 8 bookkeeping).

**State at pause (2026-10-16, uncommitted on top of `7bdb4fd`).** Done since
the section above: the timer settle and DRAM decoder-replica fixes; DRAM
retention became a profile value (`dram_device_retention`,
`dram_endpoint_retention`: `unit` 100,000 and 10^9 ticks as before;
realistic 64 ms per the Micron tREF) with the limit widened to 10^12 ticks;
all three table files regenerated to a fixed point and verified identical
(before the retention change, which does not affect timing). Checks on that
state: 30 `unit` memory/cache scenarios passed; under `embedded` and
`desktop` all realistic memory scenarios passed except the DRAM 16-bit mask
oracle, which failed on the old 100 ns retention (the reason for the
retention change; not yet rerun). Open before committing 4.4:
1. `DelayRuleTest` at `unit` reports 2 rule breaks: compact
   `memory.nor-flash.controller.8/16` change REQ_READY 0 ticks after a clock
   edge where the gates take 7. Not yet investigated (likely the compact
   controller's handshake timing after the regenerated tables).
   `memory.sram.controller.16` is below target with no floor; the ROM
   controller floors can be removed.
2. Rerun the realistic memory scenarios and the CPU panel under all profiles
   on the current build.
3. Then commit, update STATUS and CHANGELOG, and do the plan check before
   phase 5/6.

## 2026-10-16 — Phase 4.4 results and acceptance

**NOR controller delay-rule break, resolved.** The gate-level DRAM/NOR
controllers have gate lists (delay twins) since 4.3 (the regenerated
gate-list file dropped them). For the NOR controllers at 8/16 bits the twin,
after X or Z inputs in the rule's samples, claimed REQ_READY had settled
before the edge at which the compact part changed it, so the compact part
published 0 ticks after the clock where the gates take 7. Fix in
`BasicComponent::publishTime`: a change that the gate list says settled
before now is timed by the table instead (never earlier than the gates).
Floors: `memory.sram.controller.16` recorded (worst share 86 %, LAUNCH),
`memory.rom.controller.8` removed (meets the targets).

**Checks on the final 4.4 state (all passed):**
- `unit`: DRAMTest, MemoryDeviceContractTest, MemoryEndpointDRAMTest,
  MemoryEndpointTimingTest, MemoryDeviceTimingTest,
  NativeMemoryDeviceContractTest, CacheReadTest (the other 23 focused unit
  memory/cache scenarios passed earlier on the same timing; the retention and
  twin changes do not alter `unit` behavior of those parts).
- `embedded` and `desktop`, 18 memory scenarios each: SRAM, SRAM access
  control, ROM, ROM address control, DRAM, DRAM command control, NOR, NOR
  command control, device contract (including the 16-bit DRAM and NOR mask
  oracles), native device and contract, controller, the four timed
  endpoints, scalable endpoint, endpoint contract.
- `DelayRuleTest` over every part (default run): 0 rule breaks, 0 parts below
  target without a recorded exception, 6 with one.
- Tables: all three profiles at a fixed point, `--verify` identical.

**CPU panel (advanced core, two-bit, oracle-verified, period = certified
safe clock rounded to even):**

| Memory | `unit` (ticks) p19/20/21/22 cycles | `embedded` (ps) | `desktop` (ps) |
| --- | --- | --- | --- |
| FF | 100: 103/61/110/79 | 4508: 103/61/110/79 | 3140: 103/61/110/79 |
| SRAM | 102: 472/381/612/459 | 4608: 405/327/525/395 | 3210: 405/327/525/395 |
| DRAM | 106: 1086/868/1375/1108 | 4808: 3139/2219/3721/2817 | 3348: 3110/2046/3449/2704 |
| ROM | 102: 472/381/612/— | 4608: 405/327/525/— | 3210: 405/327/525/— |
| NOR | 104: 604/436/732/— | 4726: 1877/1462/2385/— | 3290: 1877/1462/2385/— |

All 54 runs completed and verified. The realistic DRAM and NOR numbers use
placeholder intervals with a per-row term over 32,768 rows (about 100 ns per
activate or read) and are not physical results; per the plan check after
phase 4 the external devices get absolute datasheet timing in phase 6.

**Phase 4 acceptance.** Memory contract, timing and endpoint tests pass on
all profiles (tick-driven timing tests on `unit`, per the phase 3 rule); the
self-timed delay-rule exceptions of the old C++ controllers are gone (the
remaining LAUNCH floors are documented clocked-interface shares). Phase 4 is
complete. Next: phase 5 as revised in the plan record (READY after data,
structural multi-subarray on-chip SRAM and its size formula).

## 2026-10-16 — Phase 5: on-chip geometry and completion (in progress)

Phase 4.4 was committed as `ecee655`.

**5.1 READY after data.** The SRAM timer gains a fourth stage (two memory
primitive delays for the sense amplifier and output buffer, two gates for
the sense-enable AND and margin) and READY comes from it; the ROM timer gains
a third stage (one memory primitive for the output buffer, column-mux depth
plus two gates). READY no longer precedes READ_DATA, which phase 7 needs for
directly driven cache banks. The compact fallbacks follow.

**5.2 Multi-subarray on-chip SRAM (assistant decisions).**
- The gate-level SRAM device keeps every bitline to at most
  `SRAM_SUBARRAY_ROWS` = 32 rows. A larger device builds subarrays, each an
  ordinary SRAM device with its own self-timing, plus a global decoder on the
  high word-address bits (CE_N per subarray), global wires
  (`MemoryWire`, a bus delay leaf; `sram_wire_per_side` per subarray of grid
  side, `unit` 2 ticks, realistic 0.5 FO4 placeholder) and a multiplexer
  tree returning READ_DATA and READY. Out-of-range addresses go to subarray
  0 with its top address bit set, so it answers X with READY like a single
  bank. The public contract (`memory.sram.device.W`) and pins are unchanged.
  32 rows rather than 64 so that the 4,096-cell gate-level limit still
  reaches 8 subarrays (512 words at W=8) for the held-out check.
- Size formula (`sramSubarrayEstimate`): each delay d(N) = d1 + a log2 N +
  b (side(N) − 1), side = ceil(sqrt N); a and b fitted exactly from the tables
  proven at 1, 2 and 4 subarrays (64, 128, 256 words in the default two-word
  layout). SRAM device and endpoint tables are now also proven at 256 and
  512 words; an exact table is used wherever it exists, the formula beyond
  (other layouts keep the per-doubling estimate). `SRAMSubarrayFormulaTest`
  (new CTest) checks the 8-subarray prediction against the 512-word table in
  all three profiles within ±10 %. `SRAMSubarrayTest` (new scenario) runs a
  256-word, four-subarray device in both fidelities.

## 2026-10-16 — Phase 5 results and acceptance

**Formula correction (replaces the formula stated in the design section
above).** The first fit, d(N) = d1 + a log2 N + b (side − 1) with a and b
fitted at 2 and 4 subarrays, overshot the held-out 8-subarray table by up to
13 % on the main paths (embedded WORD_ADDR→READY: formula 3266 ps, proven
2886) and 26 % on a short `unit` path: it read the one-time partition cost
(1 → 2 subarrays: decoder, output buffer; +562 ps of the series 1990, 2552,
2704, 2886) as wire growth. Every path through the array crosses exactly two
global wires, whose delay is a profile value, so the wire term is no longer
fitted: d(N) = d(4) + a (log2 N − 2) + 2 sram_wire_per_side (side(N) −
side(4)), a = d(4) − d(2), unchanged where the delay does not depend on the
array. `SRAMSubarrayFormulaTest`: 48 delays at 8 subarrays in all three
profiles, largest deviation 0 %. The exact match says the formula reproduces
how the gate-level array grows; whether that array is physically realistic
is the CACTI comparison of phase 6.

**Compact SRAM withdrawal fixed.** Under `embedded`, `DelayRuleTest` found the
compact SRAM device withdrawing its old word (to Z) at the table's minimum
CE_N/WE_N→READ_DATA delay, 20 ps before the gates in some transitions: a
transition-specific path can be slower than the table's minimum. Holding the
word until the final value broke `SRAMTimingTest` (a superseded access must
withdraw promptly). The compact part now withdraws at the later of the
table minimum and the full gate path of that withdrawal in
`SRAMAccessControl` (restart: change detector, condition gate, OR tree,
STEADY, STARTED, stage reset, output-enable tree, buffer; deselect: the CE_N
settle line). The same kind of fix may be needed elsewhere when a compact
part acts at a table minimum; the full `DelayRuleTest` under `embedded` is
the check (memory parts now pass under all three profiles).

**Partitioned SRAM bring-up.** Deselected outputs read X (multiplexers pass no
high impedance): a global output buffer, enabled by the selected subarray's
READY with chip select, output enable and not writing, now drives the data.
An unknown select bit makes the gates' READY unknown; the compact model
follows (READY X for an unknown address above the subarray bits).

**Checks, all passed unless noted.**
- Tables: all three profiles at a fixed point, `--verify` identical, 610
  entries (9 new: SRAM device, endpoint and scalable endpoint at 256 and 512
  words where they fit 4,096 cells).
- `unit` and `embedded`: SRAMTest, SRAMSubarrayTest (256 words, four
  subarrays, both fidelities compared), SRAMAccessControlTest,
  SRAMStorageBankTest, SRAMContractTest, ROMTest, ROMAddressControlTest,
  MemoryDeviceContractTest, NativeMemoryDeviceTest and contract,
  MemoryControllerTest, SRAM and ROM endpoints, scalable endpoint, endpoint
  contract. `unit`: SRAMTimingTest, MemoryEndpointTimingTest, CacheReadTest,
  four cache scenarios. `SRAMTimingTest` fails under `embedded` as expected
  (10-tick pulses; tick-driven test).
- `DelayRuleTest`: SRAM/ROM devices, controllers and endpoints, default run
  passed (4 recorded floors); SRAM device and endpoint `--report` under all
  three profiles: no breaks.
- CPU runs (advanced, two-bit, oracle-verified, before the withdrawal fix,
  which does not touch the fused endpoint): `unit` SRAM programs 19/20/21 at
  606/489/786 cycles (period 102; were 472/381/612 before READY-after-data),
  ROM 472/381/612 (unchanged); `embedded` SRAM 472/381/612 (period 4608),
  ROM 405/327/525.

**Phase 5 acceptance (revised gate):** structural and compact agree at every
structural size tested (64-word and 256-word scenarios, contract tests);
held-out size within ±10 % (0 %); access grows with size (embedded
WORD_ADDR→READY 1990 → 2886 ps from 1 to 8 subarrays); checks pass under
`unit` and `embedded`. Phase 5 is complete.

**Plan check after phase 5.** On track; one observation for phase 6. The
CPU panel's SRAM memory (256 KiB) is a 1,024-subarray on-chip array by this
model, and the static controller's synchronizer and handshake cost four to
six cycles per access, where a real on-chip SRAM at this clock runs with
zero or a fixed few wait states. That is the protocol artifact named in the
owner rule; phase 6 addresses it together with the DRAM command protocol
(see the plan record).

## 2026-10-16 — Phase 6 design: protocol realism, then calibration (assistant decisions)

Phase 5 was committed as `93bc5d0`. Under the owner's rule (realistic
memories, not cache-favouring ones), the protocol cycles that real
controllers do not spend are removed before the analog intervals are
calibrated, so calibration measures technology, not protocol overhead.

**6.1 DRAM/NOR command protocol.** Today a command costs, after its analog
chain ends: the sampler edge, a COMPLETE edge, a RELEASING edge, an edge at
which the controller sees BUSY low and registers the next command, and the
edge at which the device takes it (4–5 edges). The RELEASING edge stays: it
sets the open-row state before the running command's signal drops, so the
wordline never glitches (a glitch would read the row destructively). Changes:
- device: BUSY is reported low, and a new command accepted, during the
  RELEASING cycle; a FRESH bit (set at acceptance) keeps the new command from
  seeing the previous chain's stale sampled "done";
- controller: the next command is presented combinationally in the cycle the
  device is seen idle without a fault (device outputs are registered, so this
  is an ordinary single-cycle path), rather than registered first.
Result: about two edges after the chain (sampler, releasing) instead of four
or five. NOR program/erase keep BUSY polling (the real RY/BY# pin).

**6.2 SRAM/ROM counted access.** The two-stage READY synchronizer and the
handshake make every SRAM/ROM access cost four to six cycles; a real on-chip
SRAM at this clock runs with zero wait states, on-chip flash with a fixed
count chosen from the clock (STM32 LATENCY), external parts through
programmed FMC timing. The static controller instead counts
`access_cycles` (edges from START to capture, an endpoint parameter) and
captures the data at the last; it samples the device's READY at that edge as
a completion check (status Device if READY is not high), so devices still
signal completion (decision 2A) and a too-short count is caught in
simulation. The analyzer treats the device path as a multicycle path
(`ProtocolStableData`), and the required count at a clock period is
computed from the proven tables (controller launch + device access + capture
setup); experiments set it after certification.

**6.3 Calibration** of the realistic intervals from the PDF-checked tables
(external devices absolute, per-row terms 0), on-chip SRAM against CACTI
trends, refresh interval from tREF.

**6.4 Measurements:** end-to-end request-to-data latency per technology in
cycles and ns at the certified clocks, compared with the datasheets and
the STM32F7 ratios.

## 2026-10-16 — Phase 6.1–6.2 implemented

**6.1 DRAM/NOR command protocol.** Devices: a command is taken while idle or
during the releasing cycle (`IDLE = !BUSY | RELEASING`), BUSY is shown low in
that cycle, FRESH masks the stale sample for one cycle after acceptance, and
REPEAT (the same command taken during releasing) holds the command's run
signal low for one cycle so its step chain restarts; the run signal is now a
three-input AND, and the copies count two gate levels at launch. Controller:
the pending steps are registered at the starting edge; the selected step's
command is shown combinationally while the device is idle and the last
command ended without fault, the device takes it at the next edge, and the
command register records it. The cycle-exact copy recomputes its command
from the device's outputs after each edge (fused endpoint: `present`).
New scenario step `back-to-back-reads` takes the second read in the first
one's releasing cycle (DRAM and NOR, both fidelities compared).

**6.2 SRAM/ROM counted access.** `StaticMemorySequencer` counts
`access_cycles` edges (8-bit counter, so the circuit and its tables do not
depend on the value) and captures at the last; READY, read through a
`ProtocolStableData`, sets status Device if it is not high then. The
synchronizer and the "READY low before the next access" condition are gone.
`staticAccessCycles(width, technology, config, period)` gives the count
from the proven controller launch + device READY time + flip-flop setup and
two gates; the RV32I experiment sets it from its period and reports it
(`access_cycles` in the JSON). Endpoint parameter `access_cycles` (default 2,
1–255).

**First checks (unit, before table regeneration), all passed:** DRAM/NOR
device and command-control scenarios (with back-to-back reads),
MemoryDeviceTimingTest, MemoryDeviceContractTest, DRAM/NOR/SRAM/ROM
endpoints, MemoryControllerTest, scalable endpoint, endpoint contract and
timing, NativeMemoryDeviceContractTest, CacheReadTest and the four cache
scenarios. CPU runs (advanced, two-bit, oracle-verified, period 102/106/104,
old tables): SRAM programs 19/20/21 470/328/558 cycles (phase 5: 606/489/786;
4 counted cycles for the 256 KiB array), ROM 336/220/384 (472/381/612),
DRAM 818/651/1043 (1086/868/1375), NOR 537/382/645 (604/436/732).

**6.1–6.2 checks on regenerated tables (all passed).** Tables: three
profiles at a fixed point after round 2, `--verify` identical (610 entries;
the gate-list file changed). `DelayRuleTest` on the 36 DRAM/NOR/SRAM/ROM
devices, controllers and endpoints: default run under `unit` and `--report`
under `embedded`, 0 rule breaks, no part below target. Certified clocks:
DRAM and NOR now match SRAM/ROM (`unit` 102, was 106 and 104; `embedded`
4608 ps, was 4808 and 4726; `desktop` 3210 ps, was 3348 and 3290); every
timed memory is limited by IMEM REQ_READY full-cycle setup. CPU panel,
advanced core, two-bit, oracle-verified, at those clocks (54 runs, all
verified; `access_cycles` chosen by `staticAccessCycles`):

| Memory | `unit` p19/20/21/22 | `embedded` | `desktop` |
| --- | --- | --- | --- |
| FF | 103/61/110/79 | 103/61/110/79 | 103/61/110/79 |
| SRAM | 470/328/558/420 (4 cycles) | 336/220/384/292 (2) | 336/220/384/292 (2) |
| DRAM | 818/651/1043/840 | 2801/1857/3238/2597 | 2620/1747/2653/2488 |
| ROM | 336/220/384 (2) | 269/166/297 (1) | 269/166/297 (1) |
| NOR | 537/382/645 | 1877/1462/2385 | 1810/1408/2298 |

The `unit` SRAM cycles equal the published pre-phase-4 values (a
coincidence of four counted cycles matching the old sequencer's timing,
not a reuse). Realistic DRAM/NOR remain dominated by the placeholder
per-row terms over 32,768 rows; 6.3 replaces them.

## 2026-10-16 — Phase 6.3 reference data and decisions

Phase 6.1–6.2 was committed as `b30744a`.

**CACTI 7 (HewlettPackard/cacti, commit 1ffd8df, built locally).** Plain RAM
("ram" type, no tag), one bank, direct, 4-byte block, 32-bit I/O,
itrs-hp cells and periphery, delay-weighted objective:

| Size | 65 nm access / cycle (ns) | 32 nm access / cycle (ns) |
| --- | --- | --- |
| 512 B | 0.336 / 0.251 | 0.141 / 0.106 |
| 2 KiB | 0.370 / 0.303 | 0.157 / 0.131 |
| 8 KiB | 0.485 / 0.416 | 0.215 / 0.194 |
| 32 KiB | 0.672 / 0.681 | 0.375 / 0.298 |
| 128 KiB | 1.374 / 1.029 | 0.614 / 0.650 |
| 256 KiB | 1.863 / 1.029 | 0.835 / 0.650 |

**Findings.**
1. The gate-level asynchronous SRAM is about ten times slower than CACTI
   (`embedded`, 512 B: 3.37 ns address to data). The address-transition
   detector alone (XOR, a six-level OR tree over 43 inputs, STEADY, STARTED)
   is about 560 ps, more than CACTI's whole access; the rest are replica
   chains of FO4-scaled ANDs and placeholder intervals. This is the
   structure of an asynchronous (ATD) SRAM, which real external chips use
   (10 ns tAA for the cited CY7C1069G), not of an on-chip macro.
2. CACTI's 32 nm/65 nm ratio is about 0.45, while the profiles' FO4 ratio is
   0.70 (21/30 ps). The `desktop` FO4 interpolation is slow relative to
   CACTI's ITRS-HP 32 nm devices; recorded for a later look at the
   `desktop` gate delays, not changed now (the `embedded` profile is the
   default realistic one).

**Decisions (assistant, under the owner's realism rule).**
- **Two SRAM roles.** The existing asynchronous device models an external
  asynchronous SRAM chip; it is calibrated to its datasheet (tAA 10 ns),
  with the off-chip pad and board delay as a leaf (plan decision 3). An
  **on-chip SRAM** is a clocked macro: inputs registered at the rising edge,
  wordline and sensing self-timed from the clock in the high phase,
  precharge in the low phase, an output latch holding the data and READY
  until the next access. No transition detector. It is selected by a
  `clocked` device parameter (pins add CLK); the on-chip array is compared
  with CACTI (±20 %). The CPU panel's `sram` and the cache banks use the
  on-chip macro; this is what real microcontrollers (on-chip SRAM, zero wait
  states) and caches do.
- **Order.** 6.3a external devices (SDRAM, NOR, asynchronous SRAM) from the
  PDF tables, absolute intervals, per-row terms 0, literal NOR
  program/erase, DRAM rows up to datasheet size; 6.3b the clocked on-chip
  SRAM and its CACTI calibration; 6.4 latency measurements.

## 2026-10-16 — Phase 6.3a: external devices from datasheets (implemented)

DDR4 values were read from the Micron MT40A1G8 datasheet (Table 159
DDR4-3200 speed bins, Table 51 refresh): tAA = tRCD = tRP = 13.75 ns, tRAS
32 ns, tWR 15 ns, tRFC1 350 ns (8 Gb), tREFI 7.8125 µs, 1 KB page per x8
chip. Profile values (ps):

| Interval | `embedded` (SDR -6A) | `desktop` (DDR4-3200) | target |
| --- | --- | --- | --- |
| ACTIVATE: wordline + sense + restore | 3000 + 12000 + 3000 | 2500 + 9000 + 2250 | tRCD |
| READ | 18000 | 13750 | CL / tAA |
| WRITE: on + off + recovery | 6000 + 3000 + 3000 | 7500 + 3750 + 3750 | tWR |
| PRECHARGE: close + precharge + equalize | 3000 + 12000 + 3000 | 2500 + 9000 + 2250 | tRP |
| REFRESH extra restore (+ rest of tRFC) | 27000 (+0) | 20500 (+304250) | tRFC |
| tREFI per REFRESH | 7,812,500 | 7,812,500 | tREF/8192 |
| row of a 32-bit channel | 512 words (2 x16) | 1,024 words (4 x8) | page size |

DRAM sense and NOR read per-row terms are 0 (external devices, plan check
after phase 4). NOR (both): tACC 120 ns, program 150 µs, erase 410 ms
(typical values; literal, limits widened to 1 s and 10 s). REFRESH gains an
optional full-restore step (to tRAS), so SDR's tRFC = activate to tRAS +
tRP = 60 ns exactly; DDR4's REFRESH covers many rows per command, which the
extra 304 ns stands for (our REFRESH refreshes one row; with 128 or 64 rows
per 256 KiB experiment memory every row is refreshed every 1 ms or 0.5 ms,
well inside 64 ms). Row geometry up to 1,024 words is allowed for compact
devices (16 for gate-level); rows longer than 16 use the 16-word layout's
proven tables (only the address-field wiring and a shorter open-row
comparator differ). The RV32I experiment takes the DRAM row size and the
refresh rate (cycles = tREFI / period, at least 24) from the profile; `unit`
keeps its 2-word rows and 96-cycle refresh. Under the realistic profiles
each REFRESH-busy fraction is tRFC/tREFI (0.8 % SDR, 4.5 % DDR4).

Known model limits kept: one DRAM bank (the parts have 4 or 16); tRAS is
approximated (ACTIVATE completes at tRCD with a short restore; only REFRESH
waits for the full restore), which can make a PRECHARGE closely after an
ACTIVATE up to tRAS − tRCD − CL early; NOR page mode not modeled.

## 2026-10-16 — Phase 6.3b design: clocked on-chip SRAM (assistant decisions)

CACTI 7 breakdown at 65 nm (ns): 512 B: decoder + wordline 0.217, H-tree
output 0.031, rest (bitline, sense, mux) about 0.09, total 0.336; 8 KiB:
0.305, 0.052, about 0.13, total 0.485; 256 KiB: H-tree input 0.320, decoder +
wordline 0.376, H-tree output 0.985, rest about 0.18, total 1.863 (32 mats).

Compared with the gate-level SRAM at `embedded`: the row decoder (about
0.2 ns) is close to CACTI's decoder + wordline, and the global wire per
subarray side is of the same order as CACTI's H-tree per mat. Two costs are
artifacts of our structure, not of the technology:
1. the address-transition detector (about 560 ps), which an on-chip clocked
   macro does not have;
2. the phase-5 return path, a binary 2:1 multiplexer tree adding about 150
   ps per doubling of subarrays, where CACTI's decoder grows by 0.16 ns from
   512 B to 256 KiB and large arrays grow by wire. Real arrays drive a
   shared data bus from the selected mat.

Decisions:
- **Clocked mode** (`clocked=1` on the SRAM device and endpoint): no change
  detector and no settle lines; inputs come from the controller's registers
  and are stable before the edge; a rising-edge pulse generator (CLK and a
  delayed inverted copy) starts each access and covers precharge; the stage
  chain (row enable after the decoder replica, sense after the bitline
  interval, READY after the sense and output path) runs until the next edge,
  so data and READY hold for the controller's capture at the next edge
  (`access_cycles` = 2 for a macro whose access fits a period).
- **Shared return bus**: subarray outputs drive a tri-state bus
  (`MemoryBus` leaf: the single driven input, X on conflict, delay of the
  wire) instead of the multiplexer tree; the subarray formula is refitted
  and rechecked at the held-out size.
- **Calibration**: `sram_wire_per_side` against CACTI's H-tree, precharge and
  bitline intervals against CACTI's remainder; compared quantity is
  edge-to-data array time against CACTI access time; the residual at each
  size is reported (target ±20 %; a structural floor of gate-level replica
  timing may keep the smallest arrays above it, which will be stated, not
  hidden).

## 2026-10-16 — Phase 6.3a checks

Build and tables: full rebuild, three profiles regenerated to a fixed point
after round 2, `--verify` identical for `unit`, `embedded` and `desktop` (610
entries each; the DRAM REFRESH chain and the memory leaf values changed the
`embedded`/`desktop` files).

DRAM/NOR test subset (cell, row, sense column, storage bank, command control,
device, NOR cell/row/bank/command control/device, MemoryDeviceContractTest,
MemoryDeviceTimingTest, NativeMemoryDeviceContractTest, DRAM and flash
endpoints, endpoint contract and timing, scalable endpoint): all 19 pass
under `unit`. Under `embedded` the first run found:
- MemoryEndpointFlashTest failed (`write-first-wait-high-0` not idle before
  its deadline) and NativeMemoryDeviceContractTest and
  MemoryEndpointContractTest ran past a 900 s limit. Cause: literal flash
  times. A 150 µs program or 410 ms erase is longer than a scenario action's
  drain deadline, and clock-driven checks would need about 32,500 (program)
  or 89 million (erase) cycles at 4.6 ns. These tests check the protocol, not
  the datasheet times.
- Decision: the functional flash endpoint scenarios (`endpointSpec`, the
  write-history check) set explicit 4,000/5,000-tick pulses, as the endpoint
  timing rig already did. The profile defaults stay literal; the literal
  values are checked as profile data and by the CPU panel, which only reads
  flash.
- After the change all three pass under `embedded` and `unit`.
  MemoryDeviceTimingTest and MemoryEndpointTimingTest remain tick-driven
  `unit`-only tests (known limit; profile-derived periods are phase 8 work).

## 2026-10-16 — Phase 6.3b implementation decisions (assistant)

Made while implementing the clocked macro designed above:
- **Combinational presentation.** With `clocked=1` the static sequencer shows
  the request to the macro while START is high: CE_N and WE_N low for that
  cycle only, and address, data and mask taken straight from the request. The
  macro registers it at the starting edge. `access_cycles` still counts edges
  from START to the capture, so a macro whose access fits one period answers
  with one counted cycle. This is how an on-chip SRAM sits in a pipeline: the
  address is formed before the edge and the data is used in the next cycle.
  The asynchronous path keeps its registered request.
- **Access token instead of an address-transition detector.** The clocked
  control registers address, data, mask and WE_N at an edge with CE_N low and
  toggles a token bit. The MemoryTimer watches only the token: a new
  `detected` parameter means the first N inputs are watched and the rest are
  only held through the settle lines. The registered inputs therefore reach
  the array only after the previous access has been cleared, which avoids a
  wordline glitch under a still-active write strobe. The stages hold
  (data and READY too) until the next access; READY stays low before the
  first one. Power-on reset clears the registers (the macro has no reset
  pin).
- **Return bus for both forms.** Subarray outputs drive a `MemoryBus` leaf
  (one driver passes, disagreeing drivers give X, none gives Z, delay of the
  global wire) through per-subarray tri-state buffers. The buffers are
  enabled by the live select (asynchronous) or by the select registered at
  each access (clocked: idle subarrays keep their last word). The mux tree
  and the global output buffer are gone. The clock reaches the subarrays
  over the same global wire as the address.
- **Wider arrays.** 16- and 32-bit arrays of four or more subarrays exceed
  the gate-level limit, and the panel's 32-bit SRAM had been using the
  generic per-doubling extrapolation (found while reading the lookup, not the
  subarray formula). Larger 32-bit tables now take their own two-subarray
  table plus the 8-bit array's growth from two subarrays (`widerArray`).
  Decoder, wires and bus do not depend on the word width.
- **Proven variant.** `clocked=1` is a parameter of the SRAM device, its
  controller and the endpoints, named only when set, so asynchronous keys are
  unchanged. Its tables are `...@clocked` and `...@depth=N@clocked` (N = 64 to
  512), generated by `part_delay_tables`. The device is not split per WE_N:
  its paths start at the clock.
- **Experiment.** `--memory sram` is now the clocked on-chip macro;
  `--memory sram-async` keeps the external asynchronous chip, reported as
  `sram-async`. Cache banks move to the macro in phase 7 (not yet).
- **CACTI reference matched to the profile's devices.** The first CACTI runs
  used ITRS high-performance (HP) cells and periphery. A microcontroller
  process is low-power, so CACTI was rerun with `itrs-lop` (and
  `itrs-lstp` cells). Access times (ns), size 512 B / 2 KiB / 8 KiB / 32 KiB /
  128 KiB / 256 KiB:
  - 65 nm HP: 0.336 / 0.370 / 0.485 / 0.672 / 1.374 / 1.863
  - 65 nm LOP: 0.685 / 0.752 / 0.993 / 1.482 / 2.316 / 2.749
  - 65 nm LSTP cells, LOP periphery: 0.702 / 0.771 / 0.894 / 1.687 / 2.328 / 2.962
  - 32 nm LOP: 0.297 / 0.325 / 0.392 / 0.707 / 1.014 / 1.335

  The comparison will be made in FO4 multiples, using CACTI's own FO4 per
  device type (to be printed from a patched CACTI build), so that a process
  mismatch is not read as a structural error.

**CACTI FO4 per device type** (patched CACTI 7 printing `g_tp.FO4`): 65 nm
HP 14.8 ps, LOP 30.6 ps, LSTP 54.5 ps; 32 nm HP 6.1 ps, LOP 11.7 ps. The
`embedded` profile's 30 ps FO4 is CACTI's 65 nm LOP device, so that is the
like-for-like reference. `desktop`'s 21 ps matches no CACTI 32 nm device; it
is compared in FO4 multiples. CACTI access in FO4:

| Size | 65 nm HP | 65 nm LOP | 32 nm HP | 32 nm LOP |
| --- | --- | --- | --- | --- |
| 512 B | 22.7 | 22.4 | 23.0 | 25.3 |
| 2 KiB | 25.0 | 24.6 | 25.7 | 27.7 |
| 8 KiB | 32.7 | 32.5 | 35.1 | 33.4 |
| 32 KiB | 45.3 | 48.5 | 61.3 | 60.3 |
| 128 KiB | 92.7 | 75.7 | 100.3 | 86.4 |
| 256 KiB | 125.7 | 89.9 | 136.4 | 113.8 |

In FO4 units the small arrays agree across processes (22 to 25 FO4 at
512 B); large arrays are wire-dominated and vary with how wires scale. The
earlier finding of "about ten times CACTI" compared a 30 ps-FO4 profile with
14.8 ps-FO4 HP devices; against LOP the asynchronous SRAM is 96 FO4 at 512 B
(2.89 ns address to READY, 8-bit, 512 words), 4.3 times CACTI.

## 2026-10-16 — Phase 6.3a CPU panel and delay-rule run

**CPU panel** on the 6.3a binaries (advanced core, two-bit predictor,
oracle-verified; 54 runs, all completed and verified). Certified clocks are
unchanged from 6.1–6.2 (`unit` 102, `embedded` 4608 ps, `desktop` 3210 ps;
FF 100 / 4508 / 3140). Cycles for programs 19/20/21/22, 6.1–6.2 → 6.3a:

| Memory | `embedded` | `desktop` |
| --- | --- | --- |
| DRAM | 2801/1857/3238/2597 → 752/551/920/674 | 2620/1747/2653/2488 → 752/551/920/690 |
| NOR | 1877/1462/2385 → 2212/1732/2820 | 1810/1408/2298 → 2949/2326/3777 |
| SRAM, ROM, FF | unchanged | unchanged |

`unit` is unchanged everywhere (it keeps 2-word rows, 96-cycle refresh and
its NOR values).

Reading:
- DRAM falls by 3.4–3.7× because the experiment's DRAM now has a real page
  (512 or 1,024 words instead of 2): sequential instruction fetches and
  stack accesses hit the open row. Before, almost every access paid
  PRECHARGE + ACTIVATE. This is a realism fix in the memory's favour, but
  it is the datasheet geometry, not tuning. The remaining DRAM cost is the
  CAS latency and the clocked command protocol (tAA 18 ns = 4 cycles at
  4.6 ns; 13.75 ns = 5 cycles at 3.2 ns). Embedded and desktop come out
  almost equal in cycles because the shorter DDR4 latency is spent at a
  faster clock; program 22 shows DDR4's larger refresh share (4.5 % against
  0.8 %).
- NOR grows because tACC is now the 120 ns datasheet value (26 cycles at
  4.6 ns, 38 at 3.2 ns). A real MCU hides this with a prefetch buffer or
  flash accelerator, which this model does not have (phase 7 cache work).
- Note for 6.4: `embedded` DRAM (752 cycles) is now below `unit` DRAM (818),
  because `unit` keeps its 2-word rows on purpose (fast functional tests).

**Delay-rule run under the realistic profiles did not finish.**
`DelayRuleTest --report --rounds=30` on the 18 DRAM/NOR parts hit its
3,600 s timeout under `embedded` without printing a summary; the `desktop`
run was stopped. The same set finished under `unit`. Investigation below.

**Cause: the bench stepped one tick at a time.** `DelayRuleTest`'s bench
advanced the simulator tick by tick over a window of 3 × (the longest table
delay) + 56 after each change. At `unit` that is a few hundred steps; under
the realistic profiles a tick is 1 ps, a DRAM window is about 200,000 steps
(`memory.dram.device.8` alone took 106 s at 3 rounds) and a NOR window holds
the 410 ms erase launch (about 10^12 steps, never finishing). Fix (test
equipment only): the bench jumps from one pending event to the next, since
outputs and inner parts change only at event times. Check: the old and new
bench give byte-identical reports under `unit` on
`memory.dram.controller.8` and `memory.nor-flash.device.8` (10 rounds;
3.3 s → 1.1 s). The fixed bench was compiled against the 6.3a library for
the runs below.

**Second bench defect: endpoints were skipped as "restless".** With the
event-stepping bench, 12 of the 18 parts were compared; the six DRAM and NOR
endpoints were not. A temporary trace showed the gate form still had events
pending after the watch window, so the bench took it for a part with its own
clock. The endpoint's table covers only its edge-to-output launches (83
ticks at `unit`, 1,337 ps at `embedded`), while its device finishes an
access later (NOR read 96 ticks at `unit`; DRAM ACTIVATE 18 ns at
`embedded`). The NOR endpoints had been skipped like this before this
branch, even at `unit`; 6.3a's datasheet times extended it to the DRAM
endpoints. Fix (test equipment): after the watch window the bench follows
the part until it goes quiet (budget 10^13 ticks or 10^6 events), and any
output change in that time is checked against the table as late. A truly
free-running part still exhausts the budget and is skipped.

Result, `DelayRuleTest --report --rounds=30` on the 18 DRAM/NOR devices,
controllers and endpoints (8/16/32), bench built against the 6.3b library
(6.3b changes no DRAM/NOR part):

| Profile | Parts compared | Situations | Rule breaks | Time |
| --- | --- | --- | --- | --- |
| `unit` | 18 | 7,553 | 0 | 33 s |
| `embedded` | 18 | 7,553 | 0 | 33 s |
| `desktop` | 18 | 7,553 | 0 | 34 s |

The endpoints are now compared (11–16 output changes each, 100 % of the
compact delay in every kind). `memory.nor-flash.device.16` is at 69 %
typical share in all three profiles, just under the 70 % target. It is a
self-timed device compared only with its gates; the 6.1–6.2 default run
(100 rounds) had it above target, so this 30-round sample does not reach
its slowest case. A floor of 69 with the SAMPLE reason was added.

## 2026-10-16 — Phase 6.3b first build and fixes

The 6.3b code built without errors on the first try. First tests at `unit`
(before table regeneration):
- **SRAMSubarrayTest threw** "Memory requires W=8/16/32 ...". The new
  1-bit READY driver onto the return bus used `MemoryOutputBuffer`, whose
  pins validated the width as a memory word. Fix: the buffer accepts one
  status line (width 1) as well; its behaviour does not depend on width.
- **SRAMClockedTest failed** at `readback-0`: word 0 read 0 after an
  out-of-range write. A probe traced it. The bitline pair delivers all its
  outputs after the bitline interval (about 62 ticks at `unit`), including
  the removal of the write drive and the precharge result. In the clocked
  macro the next access's wordline opened 42 ticks after the edge
  (precharge 34 + decoder), so the cell still saw the previous write's
  drive and data, and the read rewrote word 0 with them. The asynchronous
  control hides this race behind its settle window. Decision: in clocked
  mode the precharge stage lasts max(precharge, bitline interval), because
  in this model restoring a fully swung bitline takes its swing time. Under
  `embedded` this changes nothing for small arrays (precharge 120 ps ≥
  bitline 96 ps at 2 rows) and adds up to 66 ps for 32-row subarrays.
  The behavioural and fused-engine fallbacks use the same expression. A
  self-reset precharge after each access (precharge off the access path, as
  in real macros) remains a candidate for the calibration step.
- **Return-bus fixes found by SRAMClockedSubarrayTest and SRAMSubarrayTest:**
  - READY read Z before a clocked macro's first access (no subarray
    selected yet), and an asynchronous array with CE_N high and an unknown
    address drove X onto the data bus (the buffer enables were the live
    select alone; the old global output buffer had been gated by chip
    enable). Fixes: asynchronous bus enables are select AND chip enable;
    the READY line has a pull-down (`MemoryBus` option `pulled_low`), so it
    reads 0 while no subarray drives it, as a single bank's READY does.
  - After a clocked read at an unknown address, the subarrays never
    recovered: their CE_N (CE_N OR not chosen) was X at the edge, which
    loaded X into each subarray's toggling access token, and a token XOR'd
    with X stays X. Real hardware holds 0 or 1 here; the model must not
    lock up. Decision: in the clocked macro every subarray takes each
    access (they run in parallel, so no slower) and only WE_N is gated by
    the select. An unknown address then makes only the write strobe
    unknown (a pessimistic write, which is right), and the next access
    starts cleanly. The asynchronous array keeps its per-subarray CE_N.
  - A test expectation error: in the one-word geometry the last word is
    word 0, which `write-unknown` had just made unknown.

**6.3b functional checks at `unit` (before table regeneration; old tables,
`@clocked` lookups fall back to formulas).** All pass: SRAMClockedTest,
SRAMClockedSubarrayTest (256 words, 4 subarrays), SRAMTest,
SRAMSubarrayTest, SRAMContractTest (now also the clocked macro, gates
against compact, widths 8/16/32, geometries 1×1, 4×2, 8×4),
SRAMAccessControlTest, MemoryControllerTest, MemoryEndpointSRAMTest,
MemoryEndpointTimingTest, ScalableMemoryEndpointTest, NativeMemoryDeviceTest,
NativeMemoryDeviceContractTest (35 s), MemoryEndpointContractTest (161 s),
CacheReadTest (238 s), and the component, direct-mapped, scalable and
two-level cache scenarios.

**Table regeneration (first attempt, stopped) found two more problems.**
- *No clocked tables above 64 words.* The generator reported "derived,
  gated or undriven clock is unsupported" for 128–512 words: the clock
  reached the subarrays through the global wire, and the timing analysis
  takes only one undelayed clock. Supporting a delayed clock (an arrival
  offset per clocked leaf, through the static graph and the self-timed
  memory analysis) is too deep for this phase. Decision: the clock reaches
  the subarrays directly (ideal clock, the usual STA convention), and the
  request still crosses the global wire before the subarray registers. The
  wire therefore counts in the macro's input setup time, not in its
  edge-to-data access. The total per access is unchanged; it moves into the
  cycle that forms the request. The CACTI comparison will use setup +
  access. Single-bank sizes (up to 64 words) have no global wire.
- *READY before the data.* In the round-1 tables READ_DATA's latest launch
  (2,007 ps at 64 words, `embedded`) was later than READY's (1,557 ps). A
  probe of the gate form with 400 random accesses confirmed it: data
  settled 52 ps after READY at 4 words (idle → read) and 152 ps after at
  64 words (write → read). The earlier `unit` trace showed the same one
  tick. A trace of the sense path (embedded, 4 words, edge at 0): bitline
  VALID at +1,104 ps, sense enable at +1,200 (one replica gate after the
  bitline interval), the selected column's VALID through the column
  multiplexer only at +1,302, READY at +1,310, data at +1,362. The replica
  did not cover the column multiplexer (a 2:1 tree: AND 50 ps + OR 52 ps
  per level at `embedded`). The asynchronous control has the same chain.
  Fix in both: the sense stage's replica adds
  ceil(levels × (AND + OR) / AND) gates (one pass gate for one word per
  row), so sense enable and READY follow the selected column. This is a
  correctness fix: the controllers capture data at READY.
- The same trace also measures the token window: precharge starts 684 ps
  after the edge (flip-flop clock-to-Q up to 231 ps, then the timer's change
  detection, XOR + NOT + 3 AND ≈ 300 ps). This is the expected artifact
  of about 14–20 FO4, to be weighed in the calibration step.
- *Second part of the READY fix.* After adding the column multiplexer the
  64-word array still settled data 2 ps after READY (embedded, write →
  read). A trace showed the bitline stage starts at ROW_ENABLE, but the
  wordline rises only after the decoder's enable path (the enable AND and
  the balanced AND tree over the row bits, `decode_depth` gates, about
  200 ps at 32 rows). The sense replica now also covers that path, as the
  dummy row of a replica-bitline scheme does. The probe now compares data
  and READY within each read (400 random accesses per run). Worst data −
  READY gap after the fix, clocked macro:

  | Profile | 4 words | 64 words | 256 words (4 subarrays) |
  | --- | --- | --- | --- |
  | `unit` | −1 | −1 | −1 |
  | `embedded` | −30 ps | −30 ps | −30 ps |
  | `desktop` | −21 ps | −21 ps | −21 ps |

  Data now always settles one primitive delay before READY. Latest READY
  after the edge (gate form, embedded): 1,599 ps at 4 words, 1,905 at 64,
  1,965 at 256 (setup not included).
- *Static data bound.* With the fix, the regenerated tables give READY
  launches matching the probe (1,907 ps at 64 words, embedded; probe
  1,905), but READ_DATA's static worst launch is still about 100 ps later
  (2,007 ps), while the probe saw data settle 30 ps before READY in every
  read. The static analysis bounds the data path more loosely than the
  replica chain. Decision: a clocked macro's access time for choosing
  `access_cycles` (and for the fused endpoint) is the later of the two
  proven launches. This is conservative and consistent with the safe-clock
  checks, which already use the data launch.

## 2026-10-17 — Phase 6.3b tables

**Regeneration with the clocked macro** (after the READY and ideal-clock
fixes): fixed point after round 2, `--verify` identical for `unit`,
`embedded` and `desktop`, 648 entries each (610 before: the `@clocked`
device, endpoint and scalable-endpoint tables at the default size and 64 to
512 words). As planned, 16- and 32-bit arrays above the gate-level limit
have no clocked table and use `widerArray`.

**Analyzer defect found: no clocked table for `memory.sram.controller.8`**
("TRANSACTION.REQUEST_MASK.output.OUT: clock used as data"). When the
timing graph classified the root's outputs, it traced each one back
through zero-delay pass-throughs to find clock outputs, and marked every
part it passed as a clock carrier before knowing whether the origin was a
clock. The 8-bit clocked controller passes its one-bit write mask straight
to MEM_WRITE_MASK, so the mask path was marked a clock carrier and then
reported as "clock used as data". Wider masks stop the trace at once, so
only width 8 failed. Fix (`TimingGraph.cpp`): marks from an output that does
not trace to a clock root are taken back. This can add tables for other
parts that failed the same way, so the tables are regenerated again.

**Post-regeneration checks (first pass).** Rebuild clean.
SRAMSubarrayFormulaTest: 48 delays compared at 8 subarrays, largest
deviation 0 % (with the shared bus, nothing in the structure but the wire
grows with the subarray count, so the fitted formula is exact here).
`DelayRuleTest --report --rounds=30` on SRAM/ROM devices, controllers and
endpoints (8/16/32) plus the new clocked variants, at `unit` and `embedded`:
24 parts, 8,591 situations each, **4 rule breaks**:
- *Clocked device (default, 64 and 256 words).* The compact part withdrew
  its old word at the table's minimum READ_DATA launch (8 ticks at `unit`,
  342 ps at `embedded`), before the gates (10 / 514 ps; 15 / 574 ps with
  subarrays). The static minimum comes from a shorter path than the one a
  withdrawal takes. Fix, as for the asynchronous compact SRAM in phase 5:
  READY and the word are withdrawn no earlier than the gates' withdrawal
  path (token clock-to-Q, change detector XOR, STEADY NOT, STARTED and stage
  reset ANDs; for the word also the output-enable ANDs and the buffer; with
  subarrays also the bus driver and the global wire).
- *`memory.sram.endpoint.16` (asynchronous).* After the bench set
  RSP_READY to X, RSP_VALID ended X in both forms; the compact part, timed
  by its gate twin, went X at 6 ticks, the gates at 7 (embedded: 277 ps vs
  325 ps). The twin moves a flip-flop's output after the binary
  clock-to-output; the gate-level flip-flop carries an X through its latch
  loops a little later. This is a gap in the twin's X timing, reached by a
  protocol-violating X input, not caused by 6.3b; the regenerated tables
  changed the seeded sample, which now reaches it. Recorded as an open
  defect. If the default (CTest) run reaches it, it is fixed now; otherwise
  it goes to the phase 8 list.

Clocked controller and endpoint: 0 breaks (controller typical 99 %, worst
99 %; endpoint 88–90 % / 93–94 %).
- *Withdrawal, second pass.* With the token path alone (clock-to-Q
  minimum) the compact part still withdrew 2 ticks early (10 vs 12 at
  `unit`, 512 vs 514 ps at `embedded`): on a write after a read the gates
  turn the output off through the registered WE_N's settle line, and the
  registers' clock-to-Q varies. The compact part now waits for the longest
  withdrawal path (clock-to-Q maximum, then the longer of the token reset and
  the WE_N settle line, the output-enable ANDs and the buffer, plus the bus
  and wire with subarrays). Late is allowed by the rule; early is not.

**Delay-rule results after the fixes** (SRAM/ROM devices, controllers and
endpoints, 8/16/32, plus the clocked device at default, 64 and 256 words,
clocked controller and endpoint; 24 parts):

| Run | `unit` | `embedded` | `desktop` |
| --- | --- | --- | --- |
| default (CTest, 100 rounds) | 27,070 situations, 0 breaks | 28,580, 0 breaks | 28,580, 0 breaks |
| `--report --rounds=30` | 8,591, 1 break | 8,591, 1 break | not run |

The one break in the 30-round sample is `memory.sram.endpoint.16`, the
twin's X timing described above; the default runs do not reach it. Per the
decision above it goes to the phase 8 list. Clocked parts (embedded,
default run): device 94 % typical / 96 % worst (64 words 93/96, 256 words
82/87), controller 99/100, endpoint 89/92; all "timed from its gates".

**Embedded functional tests:** SRAMClockedTest, SRAMClockedSubarrayTest,
SRAMTest, SRAMSubarrayTest, SRAMContractTest, MemoryEndpointSRAMTest,
MemoryControllerTest, NativeMemoryDeviceContractTest,
MemoryEndpointContractTest pass. CacheReadTest fails its fixed 100-tick
deadline under `embedded`, as recorded in phase 4; it is a `unit` test
until phase 8 derives its periods from the profile (it passes at `unit`).

## 2026-10-17 — Phase 6.3b results and acceptance

**CPU panel** (advanced core, two-bit predictor, programs 19–22, all 66
runs completed and oracle-verified). `sram` is now the clocked on-chip
macro; `sram-async` is the external asynchronous chip used before. Other
memories are unchanged from 6.3a.

| Memory | `unit` clock, cycles | `embedded` clock, cycles | `desktop` clock, cycles |
| --- | --- | --- | --- |
| `sram` (clocked) | 220; 336/220/384/292 (2 access cycles) | 5,236 ps; 269/166/297/228 (1) | 3,680 ps; 269/166/297/228 (1) |
| `sram-async` | 102; 470/328/558/420 (4) | 4,608 ps; 336/220/384/292 (2) | 3,210 ps; 336/220/384/292 (2) |

Program 19 run time: `embedded` 1.41 µs clocked against 1.55 µs
asynchronous (9 % faster); `desktop` 0.99 against 1.08 µs (8 % faster);
`unit` 73,920 against 47,940 ticks (54 % slower). The clocked macro's
certified clock is limited by `IMEM.RSP_READY` full-cycle setup: the core's
stall and forwarding logic, then the endpoint's acceptance, then the macro's
CE_N setup, in one cycle (the asynchronous chip is limited by `IMEM.REQ_READY`
from the same core path). See the plan check.

**Clocked macro against CACTI** (proven tables; edge to data = the later of
the READY and READ_DATA launches; setup = the latest input setup):

| Size (8-bit words) | `embedded` edge to data | setup | FO4 (30 ps): access / with setup | CACTI 65 nm LOP |
| --- | --- | --- | --- | --- |
| 4 B (4 words) | 1,611 ps | 393 ps | 54 / 67 | — |
| 64 B | 2,007 | 393 | 67 / 80 | — |
| 128 B (2 subarrays) | 2,067 | 815 | 69 / 96 | — |
| 256 B (4) | 2,067 | 865 | 69 / 98 | — |
| 512 B (8) | 2,082 | 930 | 69 / 100 | 22.4 FO4 (685 ps) |

`desktop` at 512 B: 1,445 ps access, 648 ps setup = 69 / 100 FO4 (21 ps), the
same multiples. The asynchronous chip at 512 B was 96 FO4 address to READY.
The clocked access is therefore 3.1× CACTI at 512 B (4.5× with setup). From
the traces and the structure, roughly half of the access is non-physical or
serial overhead: gate-built flip-flop clock-to-Q (up to 231 ps), the access
token's change detection (XOR 120 + NOT 30 + three ANDs, about 300 ps),
precharge in the access path (120–186 ps), and replica margins (the sense
replica now covers the wordline path and the column multiplexer, about
400 ps at 32 rows). The large setup at 128 words and above is the
periphery's 32-bit address range check feeding the registered subarray
select, which a real macro (with only the address bits it needs) does not
have. Decision (plan check): reported as found, not tuned, because at the
realistic clocks the macro completes in one cycle at every proven size.

**Acceptance of 6.3b:** the clocked macro works in both forms across widths
and geometries (contract tests), its tables are proven and verified in all
three profiles, the delay rule holds in the CTest configuration (0 breaks),
READY follows the data in every read probed, and the CPU runs verify. Not
met: the CACTI ±20 % target (3.1× residual, explained above), accepted as an
honest residual by the plan check.

**Full DelayRuleTest (CTest configuration, `unit`, every part)** on the
final 6.3 state: passed, 146 parts, 155,777 situations, 0 rule breaks,
every gate-level change inside larger parts within its own table; the gate
and compact CPU cross-checks agree cycle for cycle (for example program 20
with SRAM 400 and 400 cycles). Runtime 1,217 s. The scalable endpoints and
scalable caches are "not compared" as before (their storage has no gate
form). This also confirms the bench changes (event stepping, following a
part until quiet) on every part, not only memories.

**Commit.** 6.3a and 6.3b are committed together: they share
`MemoryProtocol`, `RV32IExperiment`, `ProvenPartDelays`, the endpoint tests
and the generated tables, and the 6.3a checks above were run on the 6.3a
build before the 6.3b code was compiled.

## 2026-10-17 — Phase 6.4a: handshake decoupling and clocked-macro setup (implemented)

Two changes, made together because each needs a table regeneration:

1. **REQ_READY from registered state only** (native endpoints: SRAM, ROM,
   DRAM, NOR). Before, the transaction port's READY was
   `!RST & (!PENDING | (COMPLETION & RSP_READY)) & BACK_READY & IN_READY`,
   where IN_READY = !FULL | RSP_READY, so the consumer's RSP_READY reached
   START (and, with the clocked macro, the macro's CE_N) in the same cycle.
   Now READY = `!RST & (!PENDING | COMPLETION) & BACK_READY & !FULL`. The
   capacity stays one transaction including a held response, so no protocol
   scenario changes meaning; streaming is unchanged (a consumer that takes
   each response as it completes keeps the buffer empty through the bypass);
   after a held response a new request waits one more cycle. FINISH and the
   buffer still use RSP_READY, which only updates registered state. Applied
   identically to the gate-level port (`MemoryTransactionPort`), the compact
   controller and the fused endpoint. The flip-flop endpoint keeps its
   pass-through (an ideal test memory whose clock is the core's own, 100 /
   4,508 ps); this difference is deliberate.
2. **Clocked macro without logic before its registers.** Above 64 words the
   periphery's 32-bit range check and the subarray select decode fed the
   registered selects and a per-subarray WE_N gate before the edge (WORD_ADDR
   setup 930 ps at 512 words, `embedded`). Now every subarray gets CE_N and
   WE_N ungated, and subarray k sees the address with its select bits
   inverted where k has a one (one NOT gate), so its own range check accepts
   only its addresses; out-of-range accesses write nothing and read X in
   every subarray, as a single bank does. The bus enables are decoded after
   the edge from a periphery register holding the raw address (out of range
   selects subarray 0, keeping the contract's X data with READY high). The
   decode now runs in the access path in parallel with precharge, as a real
   macro decodes its bank after the address register. Expected setup: the
   registers' own plus the global wire.

**Correction to item 1 above (capacity).** The statement "the capacity
stays one transaction including a held response" is wrong. Accepting a new
request at the completion edge without looking at RSP_READY means a
response can stay held while one new request is in flight: the capacity is
one held response plus one outstanding request. This is correct and keeps
the order (a second completion waits, with DONE held, until the held
response is taken, through the existing FINISH backpressure); keeping
capacity one without RSP_READY would cost a bubble on every streamed access.
Scenario changes, in the shared endpoint transaction helper: after a held
response, REQ_READY rises one edge after it is consumed (not in the cycle
RSP_READY rises); a request rejected by the controller (alignment, range,
read-only, unsupported, invalid) finishes in its accepting cycle, and
REQ_READY is high right after that edge. The flip-flop endpoint keeps the
old expectations.

**Functional checks at `unit` (old tables).** Pass: SRAMClockedTest,
SRAMClockedSubarrayTest, SRAMSubarrayTest, SRAMContractTest,
MemoryControllerTest, MemoryEndpointFF/SRAM/DRAM/ROM/FlashTest,
ScalableMemoryEndpointTest, NativeMemoryDeviceContractTest,
MemoryEndpointContractTest, MemoryEndpointTimingTest, CacheReadTest and the
four cache scenarios.

## 2026-10-17 — Phase 6.4a results; endpoint throughput analysed

**Tables after 6.4a:** fixed point after round 2, `--verify` identical in all
three profiles (649 entries). Clocked macro WORD_ADDR setup at 512 words
930 → 408 ps (`embedded`), access unchanged (READY 1,982 ps, data 2,082 ps):
decoding after the edge costs nothing. Endpoint REQ_READY is now a pure
launch (262–377 ps after the edge); RSP_READY's setup (565 ps) reaches only
the buffer registers. **Delay rule** (default, 42 memory parts: SRAM, ROM,
DRAM, NOR devices, controllers, endpoints plus the clocked variants): 0 rule
breaks, 0 below target, in `unit`, `embedded` and `desktop`.

**CPU panel (SRAM rows; all runs verified):**

| Memory | `unit` | `embedded` | `desktop` |
| --- | --- | --- | --- |
| `sram` (clocked) | 220 → 158; 403/274/471/356 (3 access cycles) | 5,236 → 4,104 ps; 269/166/297/228 | 3,680 → 2,858 ps; 269/166/297/228 |
| `sram-async` | 102 → 92; 470/328/558/420 | 4,608 → 4,104 ps; 336/220/384/292 | 3,210 → 2,858 ps; 336/220/384/292 |

At `embedded` and `desktop` both SRAMs now share one certified clock, and the
clocked macro is 20 % faster in time (program 19: 1.10 µs against 1.38 µs at
`embedded`). The flip-flop memory, whose endpoint still passed RSP_READY on,
became the slowest-clocked (4,508 ps), so it gets the same rule (below).

**CTest subset:** MemoryEndpointSafeClockTest, RV32IExperimentTest,
MemoryArchitectureCleanupTest, RV32IAdvancedExamplesTest,
TwoPortMemoryArbiterTest, RV32IMemoryMigrationTest pass;
RV32IExperimentCLITest failed because it pins `--memory sram` at `unit` to
102 ticks, a marker from before `sram` became the clocked macro (it was not
in the 6.3b subsets). Updated to the current certified value, 158.

**Endpoint throughput (6.4b question, measured).** A probe streams reads
through an endpoint with REQ_VALID and RSP_READY held high (gate and compact
forms agree):

| Memory | accepts | request to response |
| --- | --- | --- |
| flip-flop | every cycle | same cycle |
| clocked SRAM, 1 access cycle | every 2 cycles | 2 cycles |
| ROM, 1 access cycle | every 2 cycles | 2 cycles |
| asynchronous SRAM, 2 access cycles | every 3 cycles | 3 cycles |

A static access costs `access_cycles + 1`: the controller takes the device's
word into its RESULT register at the counted edge, and the response is used
one edge later. Program 19 retires 66 instructions: 103 cycles with the
flip-flop memory (CPI 1.56) and 269 with the clocked SRAM (CPI 4.1).
Two ways to remove the extra cycle were examined and rejected:
- *Bypass the device's word to the response at the counted edge.* The word
  reaches the RESULT register through `ProtocolStableData`, a declared
  multicycle exception. Bypassing would put a one-cycle path behind that
  exception: device access (2.1 ns at `embedded`, 3× CACTI) plus the
  endpoint's multiplexers plus the core's own requirement on RSP_READ_DATA
  (settled 45 ticks before the edge at `unit`), against a 4.1 ns period. It
  does not reliably fit, so the registered response is what a real
  controller facing the same budget would also do (or add a wait state).
- *Overlap requests in the endpoint (pipelined bus).* The core's requester is
  one-outstanding (`RV32IMemoryRequest`), so it issues the next request only
  as it takes the previous response; overlap would gain nothing.

Decision: no change. The extra cycle is the honest data-phase budget with
this macro's access time and a one-outstanding core, not an artifact. Phase
7's cache hit path faces the same budget and must be judged by it (it must
not get a bypass the direct path is denied).

**Flip-flop endpoint, same handshake rule.** Its REQ_READY is now `!RST &
!FULL` (gate-level controller, compact controller, fused endpoint), so the
ideal reference memory no longer carries a handshake path the others do not.
Scenario expectations: after a held response REQ_READY rises one edge after
it is consumed, as for the native endpoints.

Other memories in the same 6.4a panel (66 runs, all verified): every native
memory now certifies at 4,104 ps (`embedded`, was 4,608), 2,858 ps
(`desktop`, was 3,210) and 92 ticks (`unit`, was 102), except the clocked
SRAM at `unit` (158). Cycles, programs 19/20/21/22: DRAM `embedded`
752/551/920/682, `desktop` 754/552/922/700; ROM 269/166/297; NOR `embedded`
2,413/1,894/3,081 (was 2,212/1,732/2,820: the fixed 120 ns read spans more of
the shorter cycles), `desktop` 3,284/2,596/4,212. FF unchanged at 4,508 /
3,140 ps (before its handshake change).

## 2026-10-17 — Phase 6.4b: end-to-end latency against datasheets

Probe: one read at a time through the compact endpoint (cycle-exact with the
gates), refresh off, `access_cycles` chosen as the experiment does, DRAM rows
from the profile; latency = accepting edge to the edge the response is
taken, at the 6.4a certified clocks.

| Memory | `embedded` (4,104 ps) | expectation from the datasheet | `desktop` (2,858 ps) | expectation |
| --- | --- | --- | --- | --- |
| clocked SRAM | 2 cycles (8.2 ns) | 1 access cycle + registered response | — | — |
| asynchronous SRAM | 3 cycles | `access_cycles` + 1 | — | — |
| ROM | 2 cycles | `access_cycles` + 1 | — | — |
| NOR read | 34 (139.5 ns) | tACC 120 ns ≈ 30 + 1 → 31 (+10 %) | 47 (134 ns) | 43 + 1 → 44 (+7 %) |
| DRAM row hit | 9 (36.9 ns) | CL ≈ 5 + issue and response ≈ 6.5 (+38 %) | 9 (25.7 ns) | ≈ 6.5 (+38 %) |
| DRAM, row closed | 16 (65.7 ns) | tRCD + CL ≈ 10 + 1.5 ≈ 11 (+45 %) | 17 (48.6 ns) | ≈ 11.5 (+48 %) |
| DRAM row miss | 23 (94.4 ns) | tRP + tRCD + CL ≈ 14 + 1.5 ≈ 16 (+44 %) | 25 (71.5 ns) | ≈ 16.5 (+52 %) |

NOR is within the ±25 % tolerance. DRAM is 38–52 % over what a real
controller spends: about two edges per command beyond the counted
interval, from the BUSY completion handshake (the device's interval ends
mid-cycle, the controller samples BUSY low at an edge, releases, then issues
the next command). Real SDRAM has no BUSY; its controller counts tRCD, CL and
tRP in cycles. Overstating DRAM latency would make caches look better, so by
the owner rule it is fixed before phase 7.

**Decision (6.4c): counted DRAM command timing.** The DRAM controller waits a
counted number of cycles per command, ceil(proven interval / period), taken
from the device's proven tables exactly as `staticAccessCycles` does for
SRAM/ROM; the gate-level DRAM device stays self-timed and its completion is
still checked at the counted edge (a command found not complete ends with
status Device, as for SRAM's READY). NOR keeps BUSY: real NOR flash reports
program and erase completion by status polling or RY/BY#, and its read is
within tolerance.

## 2026-10-17 — Phase 6.4c: one synchronizer edge per DRAM command (implemented)

Where the DRAM edges went (gate-level device, `DRAMCommandControl`): the
chain's end is sampled at edge e1 (`DONE_SAMPLER`), the command completes at
e2 (open row, READ_VALID, a releasing cycle), BUSY shows low only after e2,
and the next command is taken at e3. A real SDRAM takes its next command at
the first edge after the interval. A fully counted controller would need the
device to use its chain end at the edge without a sampler (a multicycle path
the analysis and the 6.1–6.2 FRESH/REPEAT logic were not built for), so the
revised decision keeps one sampling flip-flop, the synchronizer a
self-timed completion needs (owner decision 2A), and removes the second
edge: a completion seen by the sampler is shown at once (BUSY low,
READ_VALID and the output enable for a read, the new open state) and the
next command is taken at the completing edge e2. RELEASING is set only when
no command follows. Legality uses the shown open state. The analog wordline,
sense and precharge controls keep the registered open bit: on a completing
ACTIVATE followed at once by a new command, the open bit rises a register
delay after e2 while the ACTIVATE run term falls two gate levels later, so
the wordline never drops. Gate form and compact `DRAMSyncControl` changed
identically. NOR keeps its two edges (its read is within tolerance and real
NOR reports program and erase completion).

Checks at `unit` (old tables): DRAMCommandControlTest, DRAMTest,
MemoryDeviceContractTest, MemoryDeviceTimingTest, NativeMemoryDeviceTest,
MemoryEndpointDRAMTest, NativeMemoryDeviceContractTest,
MemoryEndpointContractTest, ScalableMemoryEndpointTest pass (no scenario
expectation needed a change).

Latency probe after the change:

| DRAM read | `embedded` before → after | vs expectation | `desktop` before → after | vs expectation |
| --- | --- | --- | --- | --- |
| row hit | 9 → 8 cycles (32.8 ns) | +23 % | 9 → 8 (22.9 ns) | +23 % |
| row closed | 16 → 14 (57.5 ns) | +27 % | 17 → 15 (42.9 ns) | +30 % |
| row miss | 23 → 20 (82.1 ns) | +25 % | 25 → 22 (62.9 ns) | +33 % |

The remaining cost over a counted real controller is one synchronizer edge
per command plus the registered response, reported as the model's residual
(at or slightly over the ±25 % tolerance; the desktop row miss is +33 %).

## 2026-10-17 — Phase 6.4: tables, checks and the analysis's handshake models

After 6.4c and the flip-flop handshake change: tables at a fixed point after
round 2, `--verify` identical in all three profiles (649 entries).
DelayRuleTest (default) on all five technologies' devices, controllers and
endpoints plus the clocked variants (53 parts): 0 rule breaks, 0 below
target, in `unit`, `embedded` and `desktop`.

The CTest subset then found that the analysis still held three copies of the
old handshake rule (REQ_READY following RSP_READY):
- `MemoryEndpointSafeClockTest` failed ("FF endpoint oracle mismatch:
  REQ_READY"): the test's independent flip-flop endpoint oracle, its list of
  combinational dependencies (RSP_READY → REQ_READY), the fused flip-flop
  endpoint's declared one-tick contract (`TimingCircuit.cpp`) and the
  analyzer's built-in flip-flop endpoint model (`TimingGraph.cpp`,
  `TimingEngine.cpp`: REQ_READY followed RSP_READY in the holding state) all
  still had the arc. All four now say REQ_READY = !FULL; an input with no
  combinational path (RSP_READY, like REQ_WRITE_DATA) needs a shorter probe
  period (10, was 12). The test passes: 2,304 state/request cases, all 8
  arcs witnessed. Its failure message now names the period found and the
  limiting constraint.
- `RV32IExperimentCLITest` failed with "EXPERIMENT.IMEM is in a handshake
  state the clock analysis did not cover" on the gate-level DRAM run. The
  safe-clock analysis enumerates the endpoint's handshake states
  (`MemoryTimingState.cpp`, `sequencedStates`); its copy of READY was
  updated, but a richer diagnostic (now in the failure message: the
  endpoint's REQ_READY, RSP_VALID, RSP_READY, the requester's register values
  and the model cases with that handshake) showed a deeper gap: the model's
  sequencer had three states (idle, working, done), while a DRAM sequencer
  can show a result while a refresh is due (BACK_READY = !ACTIVE &
  !REFRESH_PENDING) or already running. With the capacity-two handshake and
  6.4c's timing this became reachable (the requester was draining an
  abandoned fetch). The model now keeps DONE and the phase apart (idle,
  working, blocked by a due refresh; DRAM only), with the real sequencer's
  transitions. The gate-level checks pass: every cycle's state covered for
  DRAM (1,144 checks), FF (170) and SRAM (668), all verified with 0 setup
  violations.

Because the analysis's state model feeds the safe-clock certification, the
6.4c panel that had started with the old model was stopped and is rerun.

**Further CTest finding.** RV32IExperimentCLITest expected `--safe-clock
--fidelity behavioral` to be refused. It was refused because the compact
core's table and the flip-flop endpoint's RSP_READY → REQ_READY arc formed a
combinational loop, on which the analysis fails closed. With the arc gone the
compact core certifies at 94 ticks, the same period as the gate-level core
with the same memory; the test now requires exactly that agreement.

**CPU panel after all of 6.4** (66 runs, all verified):

| Memory | `unit` clock | `embedded` | `desktop` | program 19 cycles (`unit` / `embedded` / `desktop`) |
| --- | --- | --- | --- | --- |
| FF | 94 | 4,104 ps | 2,858 ps | 103 / 103 / 103 |
| SRAM (clocked) | 158 | 4,104 | 2,858 | 403 / 269 / 269 |
| SRAM (asynchronous) | 92 | 4,104 | 2,858 | 470 / 336 / 336 |
| DRAM | 92 | 4,104 | 2,858 | 654 / 683 / 685 |
| ROM | 92 | 4,104 | 2,858 | 336 / 269 / 269 |
| NOR | 92 | 4,104 | 2,858 | 537 / 2,413 / 3,284 |

Every memory, the flip-flop reference included, now certifies at one clock
per realistic profile (the core's own limit): the handshake no longer costs
any memory a longer period. DRAM program 19 at `embedded`: 752 → 683 cycles
(one edge per command, 6.4c).

## 2026-10-17 — Phase 6.5: core sweep

Script: certify the safe clock per core × memory (program 20), then run
every program at that clock, oracle-verified. The single-cycle and baseline
cores accept only zero-wait FF endpoints ("Single-cycle and baseline require
zero-wait FF endpoints", by design), so they run with FF; the advanced core
runs with every memory (two-bit predictor), and with the four other
predictors on the default `sram`. Programs 1–8 and 22 store data; ROM and NOR
flash are read-only, so those runs end in an oracle mismatch at the first
store (checked: program 7's mismatch is at PC 16, `sw x3, 0(x1)`) and are not
counted as failures.

**`embedded`, programs 1–22:** 246 runs verified, 0 failures apart from the 16
store-on-read-only runs. Certified clocks: single-cycle FF 7,164 ps,
baseline FF 3,354 ps, advanced 4,104 ps with every memory. **`desktop` and
`unit`, programs 19–22:** every run verified (30 each); clocks single 4,986 /
166, baseline 2,334 / 80, advanced 2,858 / 94 (FF), 158 (clocked SRAM, limited
at `unit` by DMEM REQ_VALID through the macro's setup) and 92 (other memories).

Advanced core, two-bit, `embedded`, cycles per program:

| Program | FF | SRAM | SRAM async | DRAM | ROM | NOR |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 43 | 114 | 146 | 315 | — | — |
| 7 | 71 | 212 | 276 | 600 | — | — |
| 9 | 6 | 10 | 12 | 28 | 10 | 74 |
| 17 | 77 | 223 | 296 | 667 | 223 | 2,559 |
| 19 | 103 | 269 | 336 | 683 | 269 | 2,413 |
| 21 | 110 | 297 | 384 | 831 | 297 | 3,081 |
| 22 | 79 | 228 | 292 | 616 | — | — |

(Full table in the sweep log; every program has the order FF < SRAM ≤ ROM <
SRAM async < DRAM < NOR.)

## 2026-10-17 — Phase 6 acceptance

| Criterion (plan) | Result |
| --- | --- |
| Each technology's times within a stated tolerance of its source | NOR read +7–10 % (±25 % met); DRAM end-to-end +23–33 % over a counted real controller (met at `embedded` within rounding, desktop row miss +33 % not met); clocked SRAM 3.1× CACTI at 512 B (±20 % not met). Both residuals are explained (one synchronizer edge per DRAM command under decision 2A; gate-built registers, access token and serial precharge in the macro) and accepted by the plan checks as reported, not tuned |
| Order SRAM < DRAM ≤ NOR read | Met: 8.2 / 32.8–82 / 139.5 ns at `embedded` |
| End-to-end latency against real systems | Done against datasheets (6.4b, 6.4c). The STM32F7 system check was not done: no authoritative table was read (searches found only generic FMC facts); recorded as open, not claimed |
| Protocol cycles a real controller would not spend | Reviewed: DRAM release edge removed (6.4c); static controller synchronizer removed (6.1–6.2); same-cycle RSP_READY in REQ_READY removed (6.4a); registered response kept (justified, 6.4b); L1 hit latency is phase 7 |
| CPU runs | 66-run panel and 306-run core sweep, all verified |

Phase 6 is accepted with the residuals above.

**Final check before the 6.4–6.5 commit:** full DelayRuleTest (CTest
configuration, every part): passed, 146 parts, 155,777 situations, 0 rule
breaks; the gate and compact cores agree cycle for cycle (advanced program
20 with SRAM 400 / 400), and the compact advanced core's clock equals the
gates'. Runtime 995 s.

## 2026-10-17 — Phase 7 design: cache banks driven directly (assistant decisions)

The current `Cache<W>` controller is a serial phase machine (TAG_SEND,
TAG_WAIT, DECIDE, DATA_SEND, DATA_WAIT, DONE; a second serial tag pass for
way 1; fill word by word; then a replayed data read). Its tag and data
stores are full memory endpoints, so each phase is a whole request/response
transaction; that is why the 2026-10-16 probe measured a 14-cycle L1 hit.
Decisions for the rebuild:
- **Banks.** Per way a tag bank (one 32-bit word per set) and a data bank
  (sets × words per line), driven directly as native devices: `sram`
  technology is the clocked macro (compact automatically above the
  gate-level cell limit), `ff` the flip-flop device. Valid bits and the LRU
  bit stay in flip-flops, as now.
- **Lookup in one cycle, both ways in parallel.** The banks' address is the
  front end's address in the accepting cycle and the held request address
  after it: the macro registers it at the accepting edge, a flip-flop bank
  reads it combinationally after that edge. The next edge compares both
  ways' tags, takes the hit way's word into the result register, and the
  response is used one edge later: a two-cycle hit, the same budget as the
  direct SRAM endpoint (registered response, no bypass, REQ_READY from
  registered state). The path from the banks through the compare and the way
  select to the result register is ordinary logic seen by the timing
  analysis, so a certified clock covers it.
- **Miss.** The victim line's valid bit is cleared at the miss decision;
  each fill word returning over the backing endpoint (MEM_ port, unchanged)
  is written into the victim's data bank at the edge it arrives; the
  requested word is taken into the result register as it passes (no replayed
  read); the tag write, valid bit and LRU update go with the last word.
- **Write.** Write-through, no-allocate as now: memory first; on success a
  hit way's data bank is updated with the byte mask at the completing edge.
- **Statuses.** The banks have no error responses (their index is always in
  range); request errors and backing-memory errors keep their codes.
- **Scope.** One cache architecture (the endpoint-store design is replaced,
  not kept beside it); public examples and pages are regenerated in phase 8.

## 2026-10-17 — Phase 7a: direct-bank cache implemented; Step 1 measured

The direct-bank cache (design above) replaced the endpoint-store design in
`Cache.cpp` (new `CacheLookupPhases` and `CacheBankController`; banks keep
the names `TAG_STORE_k`/`DATA_STORE_k`, the state machine is the
controller's `PHASES` child, so layout checks keep their anchors). Checks at
`unit`: CacheComponentScenarioTest, CacheDirectMappedScenarioTest,
CacheScalableScenarioTest, CacheTwoLevelScenarioTest, CacheReadTest (77 s,
was 148 s) and RV32IMemoryMigrationTest (the cached CPU path) pass. Two
scenarios needed the 6.4a expectation for a rejected request (REQ_READY high
right after the accepting edge).

**Cache plan Step 1, `embedded` at 4,104 ps** (probe: one read at a time,
L1 = 8 sets × 2 ways × 16-byte lines, L2 = 64 sets × 2 ways × 16 B, SRAM banks,
DRAM backing with 512-word rows, refresh off; cycles from the accepting edge
to the edge the response is taken):

| Path | cycles |
| --- | --- |
| direct DRAM | 14 cold (row closed), 8 for every later read (open row) |
| L1 hit | 2 (the 2026-10-16 probe: 14) |
| L1 miss, line of 4 words from DRAM | 38 |
| L1 miss, L2 hit | 14 |
| L1 and L2 miss | 50 |

The hit path meets its target (two cycles, the direct SRAM's budget). The
Step 1 order check fails: an L2 hit (14) is slower than a direct DRAM read
(8), and an L1 miss costs almost five DRAM reads. Cause: every line fill
moves word by word, each word a whole single-word request and response,
from DRAM or from the L2. Real caches fill a line in one burst (SDRAM bursts
of 4 or 8; a Cortex-M7 refills its L1 over 64-bit AXI bursts; an L2 hands
the L1 a line in one access). Word-by-word fills are protocol cycles a real
system does not spend, against the cache; by the owner rule ("in either
direction") they are a model defect, not a result.

Also measured: DRAM is 8 cycles (33 ns) at the 244 MHz `embedded` clock, not
"tens of cycles": the Step 1 expectation was written for desktop-class
clocks. At an MCU clock (STM32F7: 216 MHz) an SDRAM row hit of a few tens of
nanoseconds is indeed under ten cycles; the expectation is corrected, not
the model.

**Decision (phase 7b): burst reads on the backing interface.** A burst read
request (op 4) returns `block_words` words of an aligned block as successive
responses. DRAM: one READ, then the next columns of the open row one per
clock, as SDRAM does after CL (the column multiplexer from the latched row,
under one cycle). Static memories (SRAM/ROM/FF): successive words every
`access_cycles` without a handshake per word. Cache: fills use bursts; a
cache serving as the next level streams a line from its data bank after one
lookup. Each layer in gate, compact and fused form with the existing
equivalence tests, and the analysis's handshake models extended to the
burst states. The campaign waits for 7b (cache plan rule: a Step 1 violation
goes back to the timing plan).

## 2026-10-17 — Phase 7b, stages 1–3: burst protocol and static-memory bursts

- **Protocol.** `Operation::ReadBurst` (op 4): the `block_words` words of
  the aligned block holding the address, from that word on and wrapping, as
  successive responses. Accepted by SRAM and ROM (DRAM in stage 4); other
  memories report Unsupported. The gate request check decodes BURST and no
  longer treats op 4 as reserved for those memories.
- **Transaction port, multi-word.** The sequencer reports LAST (the result
  shown is the transaction's last); PENDING clears, and a new request may be
  taken, only with the last word. The port tells the sequencer SPACE (no
  response held, registered). Gate port, compact controller and fused
  endpoint changed alike; the cache front end and the command sequencer
  report LAST = 1.
- **Static sequencer bursts.** BURST, a words-left counter and a word
  address that steps within the block; each word is taken at its counted
  edge (`access_cycles` per word, no handshake per word), only while the
  previous word has left the RESULT register or will at this edge
  (`!DONE | SPACE`). Using the port's FINISH there would have brought this
  cycle's RSP_READY back to the device's inputs (removed in 6.4a); under
  backpressure a word waits one cycle instead. The clocked macro is shown the
  next word's address as it starts (CE_N for a starting burst word too).
- **Analysis.** The safe-clock handshake model has LAST as a state dimension
  for burst-capable endpoints (taken both ways, an over-approximation), with
  the next burst word working or already shown after a non-last word.
- **Found on the way:** the fused endpoint updated a clocked SRAM macro's
  model before its sequence read READY (correct for asynchronous devices); a
  burst starts the macro's next access at the edge that takes a word, so the
  macro's outputs are now read as they were before the edge, as for command
  devices.
- **Checks (`unit`):** new burst scenarios (SRAM asynchronous and clocked,
  ROM, scalable SRAM: two-word blocks, the first word waiting in the
  response buffer, then one word per clock, wrapping order) and the existing
  endpoint, controller, native-device contract, endpoint contract and cache
  tests pass.
- **Tables:** `block_words` other than 2 has no proven table (the burst
  logic is structure that depends on it). Decision: a `@block_words=4`
  variant (16-byte lines of 32-bit words) is proven for the burst-capable
  devices, controllers and endpoints.

## 2026-10-17 — Phase 7b, stages 4–6: DRAM bursts, burst-filling cache, tables

- **DRAM bursts (stages 4–5).** The command control takes READ_BURST (code
  6) when `block_words` ≤ `words_per_row`: one ACTIVATE (if needed), then one
  READ per word, the column stepping within the block (wrapping) with no
  new row check, as a DRAM burst stays in its open row. A words-left counter
  ends the burst; IDLE, RELEASE and BUSY follow the final word. The compact
  mirror (`DRAMSyncControl`) does the same. `block_words` reaches the device
  only when it is not the default 2. Checks (`unit`): DRAM device contract
  and endpoint burst scenarios (block 2), passed.
- **Cache (stage 6).** The controller's sequence is now five plain state
  bits (LOOKUP, MEMORY_SEND, MEMORY_WAIT, STREAM, DONE, still under the
  `PHASES` name for saved layouts); all decisions are in the controller.
  - The request check accepts op 4 (burst read of the line, from the
    requested word, wrapping); program, erase and the other codes stay
    reserved.
  - A burst hit answers the first word at the normal hit time, then streams
    one word per cycle from the hit way's data bank (STREAM_OFFSET counter).
    A clocked SRAM data bank is given the second word in the lookup cycle and
    then the next word as each is taken, or the same word again under
    backpressure (its output is only valid for the word it was last given).
  - A burst miss forwards each fill word as it arrives. The backing
    response is taken only when the result register has room (no response
    shown, or SPACE), so backpressure reaches the backing memory instead of
    dropping words.
  - New parameter `fill` = `words` (default) | `burst`. A burst fill issues
    one op-4 request at the requested address and writes each arriving word
    at `(requested + count) mod line`.
  - **Decision: a word-by-word fill now also starts at the requested word
    and wraps** (it was word 0 first). A burst miss can then forward a word
    fill in burst order, and both fill modes bring a line in the same order.
    This is the usual critical-word-first order. The response is still given
    only when the whole line is in (no early restart): the cache takes no
    new request during a fill anyway, and early restart would only favour
    the cache. `CacheScalableScenarioTest`'s cold miss now expects word 4,
    then word 0.
  - Checks (`unit`): `CacheComponentScenarioTest`,
    `CacheDirectMappedScenarioTest`, `CacheScalableScenarioTest`,
    `CacheTwoLevelScenarioTest` and `CacheReadTest` pass. `CacheReadTest`
    gained burst cases for 8/16/32-bit words, 1/2/4-word lines, one and two
    ways, FF and SRAM data banks, both fill modes: a forwarded burst miss, a
    streamed hit with the consumer holding back every other word, a refill
    after eviction, and burst-filled single reads (one backing request per
    miss). The rig now counts backing requests taken at every edge, not only
    in its idle cycles (a forwarded burst issues further fill requests while
    the consumer takes words).
- **Tables.** `block_words` ≠ 2 now selects an `@block=N` variant in
  `provenPartDelays`, after the row layout. The generator proves `@block=4`
  for every non-FF device and endpoint at the default size, 64 and 128
  words, in the default layout and the 16-word one (the long-row DRAM case).
  DRAM in the default two-word layout has none (a four-word burst would leave
  its row). NOR flash also received block-4 tables though it rejects bursts;
  they are harmless and will be dropped at the next regeneration. Regenerated:
  fixed point after round 2, 754 entries per profile (was 649), and
  `--verify` recomputed every entry identically in `unit`, `embedded` and
  `desktop`. `DelayRuleTest` gained block-4 variants (SRAM and ROM endpoints,
  DRAM device and endpoint at 16-word rows).
- **Delay rule** (14 memory contracts plus their variants, 24 parts, default
  rounds): `unit` 29,905 situations, `embedded` and `desktop` 31,415 each;
  0 parts break the rule and 0 miss the 70 %/90 % targets in every profile,
  with no recorded exception needed.
- **Cache plan Step 1, rerun** (`probe/cache_latency.cpp`, `embedded`,
  4104 ps; L1 8 sets × 2 ways × 16-byte lines, L2 64 sets, both clocked SRAM
  banks; SDR DRAM backing (-6A) with `block_words` = 4; cycles from the
  request edge to RSP_VALID):

  | Path | word fill | burst fill |
  | --- | ---: | ---: |
  | direct DRAM, cold / open row | 14 / 8 | 14 / 8 |
  | L1 hit | 2 | 2 |
  | L1 miss, DRAM open row | 38 | 14 |
  | L1 miss, DRAM cold | 44 | 20 |
  | L2 hit after an L1 miss | 14 | 8 |
  | L1 + L2 miss (open row) | 50 | 17 |

  With bursts an L1 miss costs the DRAM access plus six cycles (lookup,
  request, three further beats, capture), and an L2 hit is no slower than a
  DRAM row hit and faster than a closed or missed row. Word fills were 4.75×
  slower per miss, a protocol artefact rather than a technology property.

## 2026-10-17 — Plan check after phase 7b (assistant)

- Step 1's order now holds: L1 hit < L2 hit ≤ DRAM row hit < L1 miss <
  L1+L2 miss. The defect that stopped the campaign (word-by-word fills) is
  removed without tuning any memory timing; DRAM, SRAM and ROM intervals are
  unchanged since phase 6.
- **Decision:** the cache campaign uses `fill=burst` with the backing
  memory's `block_words` equal to the line's words (four 32-bit words), as
  real caches fill lines with bursts. The `words` fill stays the default
  parameter (it needs no burst-capable backing) and remains available for
  comparison.
- **Decision:** no early restart (critical-word-first response before the
  line is in). It would favour the cache; it can be studied later as an
  explicit variant if wanted.
- Next: freeze the geometry and run the campaign as planned (S0–S2
  standalone, then the five CPU topologies, `embedded` then `desktop`).

## 2026-10-17 — Phase 7b: CTest subset, fixes found

Subset (`build-cleanup`, `unit`, 42 tests: table checks, timing and safe-clock
tests, memory endpoint/controller/device/contract tests, cache tests and
layout/documentation checks): 37 passed at first; after the fixes below all
pass except the documentation and saved-layout checks listed last.

- **Safe-clock state model.** The LAST dimension added in stages 1–3 was
  taken for every burst-capable memory, though the analysis only covers RV32I
  requesters, which never ask for bursts. Fixed: LAST is taken both ways
  only when the requester's op bit 2 is not a constant 0 (`mayBurst`; the
  request block's op is followed to its constant or bit joiner). Both CPU
  requesters are recognised as never bursting; the evidence line says so.
  A first version crashed on the read-only fetch request (no OP input);
  guarded.
- **`RV32IExperimentCLITest`: advanced core, clocked SRAM, `unit` safe clock
  158 → 174.** Not the state model (the period stayed 174 after the fix).
  The limit is the data write mask's setup at the SRAM endpoint, whose
  65,536-word table is an estimate for 1024 subarrays from the proven 64,
  128 and 256-word tables. Since stages 1–3 the clocked sequencer's
  request-to-CE_N path has the burst OR (`valid_start | next_word`) and the
  next-word address multiplexer, and the mask reaches it through write
  validity: REQ_WRITE_MASK setup is now 24 / 26 / 26 at 64 / 128 / 256 words
  (was 24 / 24 / 24), REQ_VALID 14 / 20 / 20 (was 14 / 17 / 17). The
  subarray estimate keeps an input whose setup does not change with size off
  the global wires, and charges one that does the per-side wire for a 32×32
  grid, hence +16. The new value is a real cost (one more gate, and the mask
  now reaches the subarray registers over the global wires); the old value
  relied on the mask path not reaching the subarrays. Re-pinned with that
  explanation. The embedded and desktop effect is in the panel below.
- **`MemoryEndpointTimingTest`** (failing since 6.4a, not run then): it
  expected a request to be taken over a held response in the same edge. Since
  6.4a REQ_READY comes from registered state only and a held response blocks
  new requests; the test now expects the held response to leave first and
  the next request to be taken in the following cycle, its response kept
  under backpressure.
- **`MemoryTransactionPortTest`** scenario did not drive the new LAST input
  (its examples were rejected as not explicitly driven); it now drives LAST
  = 1.
- **`CacheLayoutTest`** (failing since 7a, not run then): the cache
  floorplan accepts only top-level helpers named `TAG_…`/`DATA_…`; 7a's bank
  wiring added `LOW`/`HIGH` constants and `TAG0_…` gates. Each bank's
  helpers are now named after it (`TAG_0_SELECT`, `DATA_1_READ_ENABLED`,
  …) and the floorplan is found again. The saved layouts in
  `visualizer-v2/layout.json` predate 7a's controller and no longer match
  (`rel_pos` missing): regenerating them belongs to phase 8's visualizer
  work.
- **Still failing, documentation (phase 8):** `MemoryDeviceDocumentationTest`
  (ROM pin lists without READY, a device snippet) and three
  `MemoryEndpointIntegrationTest` checks: two `site-docs/` code snippets (as
  before) and the transaction port's public pin table, which lacks LAST and
  SPACE.
- **CPU panel** (advanced core, two-bit predictor, programs 19–22, six
  memories, three profiles; certified clock, then each program at it): all
  66 runs verified and completed. Against the phase 6.4 panel the only
  change is the `unit` clocked-SRAM clock, 158 → 174 ps (same cycle counts).
  `embedded` (4104 ps) and `desktop` (2858 ps) clocks and every cycle count
  are unchanged; program 19 at `embedded`: FF 103, SRAM 269, asynchronous
  SRAM 336, DRAM 683, ROM 269, NOR 2413 cycles.

## 2026-10-17 — Cache campaign: Step 1 per profile and frozen setup

**Step 1** (`probe/cache_latency.cpp`; L1 8 sets × 2 ways × 16-byte lines,
L2 64 sets × 2 ways × 16-byte lines, clocked SRAM banks, `fill=burst`; DRAM
with the profile's row length and `block_words` = 4; cycles from the request
edge to RSP_VALID at each profile's certified clock; the refresh case is 400
open-row reads with refresh at the profile's tREFI):

| Read | `embedded` (4104 ps) | `desktop` (2858 ps) |
| --- | ---: | ---: |
| L1 hit | 2 | 2 |
| L2 hit after an L1 miss | 8 | 8 |
| DRAM open row | 8 | 8 |
| DRAM closed row (first access) | 14 | 15 |
| DRAM row change | 20 | 22 |
| refresh-delayed read (1 of 400) | 14 | 15 |
| L1 miss, DRAM open / closed row | 14 / 20 | 14 / 21 |
| L1 + L2 miss, open / closed row | 17 / 23 | 17 / 24 |

The order holds in both profiles. The cache plan expected DRAM "tens of
cycles" under `embedded`; with datasheet DRAM it is 8–22 cycles at these
clocks, so an L2 hit only ties a DRAM open-row hit and beats closed-row and
row-change reads. This corrects the expectation, it is not a violation; no
delay is changed.

**Frozen for the campaign** (decided before any campaign run):

- I-L1 (read-only) and D-L1 (read/write, write-through, no-write-allocate):
  each 8 sets × 2 ways × 16-byte lines (256 B), SRAM tags and data, scalable
  storage, `fill=burst`.
- Shared L2: 64 sets × 2 ways × 16-byte lines (2 KiB), read/write, SRAM,
  scalable, `fill=burst`.
- Main memory: one DRAM endpoint, 65,536 words, the profile's row length and
  refresh interval, `block_words` = 4; A has two such stores, each with the
  whole image.
- Clock: the profile's certified core clock (4104 / 2858 ps). The analyzer
  certifies only RV32I requesters directly on an endpoint, so cached and
  arbitrated roots run at that clock as *exploratory*, with zero setup
  violations required and a check at a nearby longer period.
- Workloads: `calls`, `sort` with 64 words (512 B working set: input and
  output, above the 256-B L1 and below the 2-KiB L2), and `stream` with 1024
  words (8 KiB, above the L2). Iteration counts are set by the pilot.
- Standalone traces (S0–S2), relative to these sizes: 32 words read
  repeatedly (half the L1), 128 words for two passes (twice the L1, a
  quarter of the L2; half the L2 would be 256 words, kept as the plan's
  bound), a 2048-word stream (four times the L2), and masked stores and reads
  in the first region with one write miss.

**Found while preparing the topologies:** the two-port arbiter frees itself
after one response, and the external protocol has no LAST signal, so an L1's
burst fill through it would release it after the first word. Decision: the
arbiter gets a `block_words` parameter and counts a granted burst's beats,
as an interconnect does with a last-beat signal.

## 2026-10-17 — Cache campaign: topology builder, first runs, clock evidence

- **Topologies in the experiment runner.** `RV32IExperimentConfig::topology`
  (`--topology A|B|C-ID|D-0|D-ID`, advanced core): A = two memories, each
  with the whole image; B = both ports through the two-port arbiter to one
  memory; C-ID = an I-L1 and a D-L1 above the arbiter; D-0 = the arbiter
  above a shared L2; D-ID = both. Caches use the frozen geometry
  (`campaignCache`), fill with bursts when the memory below answers them
  (SRAM, DRAM, or an L2), and the memory's `block_words` is the line (4).
  With a topology every memory and both oracle memories get the whole image;
  the final data image is compared as before, and A's instruction copy must
  be unchanged. The `unit` profile's two-word DRAM stand-in row becomes the
  16-word layout in topologies (a burst stays in its row). Boundary counters
  per cache and the arbiter, in the measured window: reads, burst reads and
  writes sent down, response words, and cycles with both arbiter ports
  requesting.
- **Arbiter bursts.** `TwoPortMemoryArbiter(name, block_words)`: a granted
  op-4 request holds the arbiter until its `block_words`-th response word
  (BURST state, BEAT counter, reset at each grant). `TwoPortMemoryArbiterTest`
  gained burst cases (4 words at W=32, 2 at W=8, 1 at W=32): the other port
  waits, every word goes to the owner, a single read after a burst ends at
  its word.
- **Run time: the clocked SRAM macro's delay twin.** C-ID ran 20 cycles in
  9.5 s with FF memory and 0.74 s with SRAM (B: 0.06 s). A temporary counter
  (reverted) showed one 13,383-gate twin settled 14,575 times in 20 cycles:
  the cache banks are small clocked macros with gate lists, and each change
  of their inputs (a fill word passes the data banks' write data) re-settled
  the whole list. Decision: a clocked SRAM device gets no delay twin (its
  outputs change only after its clock edge, timed by its table's CLK
  launches). Without it: 0.23 s and 0.24 s. DelayRuleTest on the SRAM
  device, controller and endpoint is rerun to confirm the rule and targets
  (below).
- **First runs** (program 20): A, B with SRAM and FF verified at `unit`;
  C-ID verified at `embedded` (4104 ps) with SRAM (214 cycles) and DRAM (232).
  At `unit` with period 94, C-ID deadlocked on its first fetch: the I-L1 got
  all four burst words and never answered.
- **Cached roots and the clock.** The analyzer reports *unknown* for B and
  C-ID ("no all-state timing contract" for an endpoint whose requester is
  the arbiter or a cache). The deadlock above is a clock too short for the
  cache, not a logic fault: C-ID completes at `unit` 400, deadlocks at 200;
  completes at `embedded` 3000, deadlocks at 2400; completes at `desktop`
  2000, deadlocks at 1600 — every time with **zero** setup violations
  reported. The cache's state bits are storage parts with no proven table,
  so no setup check sees them: below its limit a cached root fails silently
  (a deadlock or, in principle, wrong data that the oracle would catch).
  Consequences for the campaign: (1) cached and arbitrated runs use the
  profile's certified core clock (4104 / 2858 ps), at least 37 % / 43 % above
  the last failing period measured for C-ID, labelled exploratory as the
  cache plan requires; (2) every run must verify against the oracle; (3) a
  nearby-period check is replaced by this measured failure point, rechecked
  for D-ID; (4) the gap — storage parts without setup checks in structural
  caches — is recorded for phase 8 (the analysis should take the cache, or
  the storage parts need tables).
- **Delay rule without the clocked macro's twin** (SRAM device, controller
  and endpoint with their variants, 9 parts): `unit` 10,361 situations,
  `embedded` and `desktop` 11,871 each; 0 parts break the rule, 0 miss the
  targets, no exception needed.
- **Shared-bus deadlock (B and D-0).** `RV32ITopologyTest` (new: a sort of
  eight words through all five topologies at `embedded`, plus program 20
  through D-ID over SRAM) found B and D-0 deadlocked; A, C-ID and D-ID
  passed. Trace: the arbiter took an instruction fetch; the core then issued
  a load and stalled on it; the fetch's response arrived but the core, stalled
  on memory, held the fetch port's RSP_READY low; the arbiter stayed with the
  fetch until it was taken, so the load was never granted. With L1s (or
  separate memories) the response lands in a per-port buffer. Decision: the
  arbiter has a one-entry response buffer per port (the bypass
  `MemoryResponseBuffer`, no added latency when the requester is ready); a
  response leaves the bus into its owner's buffer whenever that buffer is
  empty (registered FULL), as an interconnect moves a response into the
  master's response register and frees the bus. The requester's RSP_READY
  then no longer reaches the memory in the same cycle (the 6.4a rule).
  `TwoPortMemoryArbiterTest` was rewritten for this contract (a held
  response frees the bus, the waiting port is granted next; non-owner data
  is no longer forced to zero, as payload without VALID has no contract).
- **Standalone benchmark (`CachePerformanceBenchmark`, new).** S0 (DRAM),
  S1 (L1, DRAM), S2 (L1, L2, DRAM) on fresh, identical images; one request
  at a time, so every path sees the same trace; every read checked against
  a reference memory and the final DRAM image compared; boundary counters at
  the DRAM and below each cache. Traces as frozen. Registered with the
  memory benchmark runner (`tools/benchmarks/run.py memory`). The benchmark
  size cap was raised from 256 to the data layout's 1024 words (512 for
  pointer-chase) so that `stream` can exceed the L2 (`RV32IBenchmarkTest`,
  `RV32IBenchmarkCLITest` updated, both pass).
- **Open defect: the gate-level clocked SRAM macro inside the cache.** The
  first benchmark run (parts from the default profile, so gate-level banks)
  failed S2 `stream` at word 636 with a stale word; reduced to S1 reading the
  first word of 24 successive lines: line 23 hits on set 7, way 0, whose tag
  reads 0x102 (line 22's tag, just written to set 6, the same row of the
  two-word-per-row tag bank); a later read of line 7 then misses, so the
  stored tag of set 7 is really lost. The same cache with compact banks
  (the campaign's profile) is correct, with zero setup violations at the
  banks. The macro alone, in both forms, passes the same access sequence
  (read 6, idle, write 6, idle, read 7), including the cache's timing.
  Not yet explained; the leading hypothesis is a hold hazard: the cache's
  write strobe and select fall shortly after the edge, which the gate-level
  input registers may capture while the compact ones cannot. This breaks
  the lower-level-first chain for cache banks and needs its own
  investigation before results are published; the campaign uses compact
  banks (the plan's primary profile), and the benchmark now builds its parts
  with that profile (`strictAllBehavioral`, as the CPU runs). With it, the
  160-line reproduction passes in S1 and S2.

## 2026-10-17 — Cache campaign Step 2: standalone results

`CachePerformanceBenchmark` with the campaign profile (compact banks,
structural cache controllers), one request at a time, all reads and the
final DRAM images verified; 63–64 s per profile. Cycles per request (DRAM
burst reads / single reads / writes in parentheses for S0 ; S1 ; S2):

`embedded`:

```
trace/phase                req | S0 cyc/req      S1      S2 | DRAM bursts/reads/writes S0 ; S1 ; S2
small-region/cold           32 |       8.19    5.19    5.94 | 0/32/0 ; 8/0/0 ; 8/0/0
small-region/warm           64 |       8.00    2.00    2.00 | 0/64/0 ; 0/0/0 ; 0/0/0
medium-region/cold         128 |       8.05    5.05    5.80 | 0/128/0 ; 32/0/0 ; 32/0/0
medium-region/warm         128 |       8.05    5.00    3.50 | 0/128/0 ; 32/0/0 ; 0/0/0
stream/cold               2048 |       8.05    5.08    5.83 | 0/2048/0 ; 512/0/0 ; 512/0/0
masked-stores/mixed         16 |      10.94    7.06    8.29 | 0/13/4 ; 3/0/4 ; 3/0/4
```

`desktop`:

```
trace/phase                req | S0 cyc/req      S1      S2 | DRAM bursts/reads/writes S0 ; S1 ; S2
small-region/cold           32 |       8.22    5.22    5.97 | 0/32/0 ; 8/0/0 ; 8/0/0
small-region/warm           64 |       8.00    2.00    2.00 | 0/64/0 ; 0/0/0 ; 0/0/0
medium-region/cold         128 |       8.05    5.05    5.80 | 0/128/0 ; 32/0/0 ; 32/0/0
medium-region/warm         128 |       8.00    5.00    3.50 | 0/128/0 ; 32/0/0 ; 0/0/0
stream/cold               2048 |       8.03    5.26    6.07 | 0/2048/0 ; 512/0/0 ; 512/0/0
masked-stores/mixed         16 |      11.94    7.71    8.94 | 0/13/4 ; 3/0/4 ; 3/0/4
```

Reading:

- An L1 hit costs 2 cycles against 8 for an open-row DRAM read; a cold or
  streaming L1 costs about 5 cycles per word (one 14-cycle burst fill per
  four words, three hits). The streaming gain is the burst: a line costs
  about two single DRAM reads. S0 reads one word per request, as a plain
  requester on the bus would.
- The L2 adds about 0.75 cycles per request when it misses too (cold
  phases, stream) and saves when the L1 is exceeded but the L2 is not: the
  medium region's warm pass is 3.50 cycles per request in S2 against 5.00 in
  S1, with no DRAM traffic.
- Write-through: every store reaches DRAM in every path (4 writes); the
  write miss does not allocate.
- Traffic matches the expected classification exactly (one burst per L1
  miss; no L2 burst to DRAM on an L2 hit).
- `desktop` differs only slightly (DRAM rows and clocks); the stream's S1/S2
  is 0.2 cycles per request slower there.

## 2026-10-17 — Cache campaign: pilot and frozen workload inputs

Pilot (`embedded`, 4104 ps, two iterations, whole program, every run
verified against the oracle):

| Workload | B cycles | D-ID cycles | B host | D-ID host |
| --- | ---: | ---: | ---: | ---: |
| calls, size 16 | 839 | 441 | 0.3 s | 1.7 s |
| sort, 64 words | 280,003 | 77,826 | 58 s | 275 s |
| stream, 1024 words | 283,877 | 100,681 | 56 s | 338 s |

D-ID costs about 3.4 ms of host time per simulated cycle, B about 0.2 ms.

**Frozen before the panel:** `--measure-body --training-iterations 1`
with 4 measured iterations for `sort` (64 words) and `stream` (1024 words)
and 256 for `calls` (size 16; one iteration is about 24 instructions). One
run then gives the warm window (`cycles`, the measured iterations after one
warm-up) and the whole cold run (`whole_program_cycles`, warm-up included).
Seeds and other options are the generator defaults. Estimated cost: about
an hour per profile for the five topologies.

Checks after the arbiter change: `RV32IExperimentTest`,
`RV32IExperimentCLITest`, `RV32IMemoryLayoutTest` pass; `CacheLayoutTest`
fails only its saved-layout comparison (phase 8, as before).

## 2026-10-17 — Cache campaign: locality experiments added (owner request)

The owner asked whether any experiment shows locality-friendly code running
faster. Existing evidence shows the mechanisms (DRAM open row 8 vs row
change 20 cycles; L1 re-reads 2 vs DRAM 8; bursts making a stream about 5
cycles per word), but no experiment varies locality for the same work: all
standalone traces are sequential, and the three CPU workloads are not
locality variants of one another. Added, with no timing change:

- Standalone traces `stride-line` (2048 reads, one word per 16-byte line:
  every cache access misses while the DRAM row stays open) and `stride-row`
  (2048 reads, one word per DRAM row, wrapping within the memory: every DRAM
  access changes row), next to `stream` (2048 sequential reads), through S0,
  S1 and S2 in both profiles.
- `pointer-chase` with 512 nodes (4 KiB, the size of `stream`'s 1024-word
  input; the most the data layout allows for two-word nodes) through all
  five topologies in both profiles, same `--measure-body
  --training-iterations 1`, 4 measured iterations; frozen before running.

## 2026-10-17 — Cache campaign Step 3: CPU panel results (exploratory clock)

Advanced core (behavioral, two-bit predictor default), DRAM main memory,
frozen geometry and inputs, the profile's certified core clock (4104 /
2858 ps; exploratory for cached and arbitrated roots, see above). All 30
runs completed and verified against the oracle (registers, final memory
image, expected result; A's instruction copy unchanged); zero setup
violations reported (which, as recorded, does not cover the cache's state
bits). `window` = the 4 (calls: 256) measured iterations after one warm-up;
`whole` = the whole run. Per-run JSON (manifest, counters) kept in the
scratch directory; the key figures are copied here.

`embedded` (CPI, speed against B, port waits, boundary traffic in the window):

```

calls:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A         62529 12.21  1.51x     62882 |  53438   7872 |          0      0/0      |     0/0/0     0 | 0/0
B         94309 18.42  1.00x     94793 |  65582  19920 |          0      0/0      |  7167/0/512    44 | 0/0
C-ID      26321  5.14  3.58x     26627 |  13310   6356 |          0      0/512    |     0/0/512     0 | 0/0
D-0       27296  5.33  3.46x     27609 |  13310   6307 |          0      0/0      |  7167/0/512     0 | 0/512
D-ID      27820  5.43  3.39x     28156 |  13310   7855 |          0      0/512    |     0/0/512     0 | 0/512

sort:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        281202 10.14  1.99x    351546 | 228848  75692 |          0      0/0      |     0/0/0     0 | 0/0
B        559942 20.19  1.00x    699939 | 337748 177776 |          0      0/0      | 33319/0/4316  1347 | 0/0
C-ID     142446  5.14  3.93x    178363 |  56990  56961 |          0    128/4316   |     0/128/4316     0 | 0/0
D-0      150892  5.44  3.71x    189009 |  57537  55719 |          0      0/0      | 33319/0/4316     0 | 95/4316
D-ID     153511  5.54  3.65x    192759 |  56990  68026 |          0    128/4316   |     0/128/4316     0 | 8/4316

stream:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        410643 14.31  1.38x    513325 | 263688 158448 |          0      0/0      |     0/0/0     0 | 0/0
B        567601 19.77  1.00x    709582 | 360588 159144 |          0      0/0      | 36895/0/4096  1662 | 0/0
C-ID     185577  6.47  3.06x    232127 |  65598  87180 |          0   1024/4096   |     0/1024/4096     0 | 0/0
D-0      194892  6.79  2.91x    243780 |  65717  88183 |          0      0/0      | 36895/0/4096     0 | 1030/4096
D-ID     201013  7.00  2.82x    251439 |  65598 102616 |          0   1024/4096   |     0/1024/4096     0 | 1024/4096

```

`desktop` (CPI, speed against B, port waits, boundary traffic in the window):

```

calls:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A         65944 12.88  1.57x     66299 |  53408   8872 |          0      0/0      |     0/0/0     0 | 0/0
B        103331 20.18  1.00x    103840 |  67751  23012 |          0      0/0      |  7167/0/512   524 | 0/0
C-ID      28499  5.57  3.63x     28810 |  13310   8534 |          0      0/512    |     0/0/512     0 | 0/0
D-0       29144  5.69  3.55x     29462 |  13310   8155 |          0      0/0      |  7167/0/512     0 | 0/512
D-ID      29665  5.79  3.48x     30006 |  13310   9700 |          0      0/512    |     0/0/512     0 | 0/512

sort:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        302048 10.89  2.08x    377524 | 228737  85255 |          0      0/0      |     0/0/0     0 | 0/0
B        628588 22.67  1.00x    785739 | 355878 204696 |          0      0/0      | 33319/0/4316  8257 | 0/0
C-ID     157293  5.67  4.00x    196962 |  56990  71808 |          0    128/4316   |     0/128/4316     0 | 0/0
D-0      165890  5.98  3.79x    207712 |  57536  70718 |          0      0/0      | 33319/0/4316     0 | 94/4316
D-ID     168220  6.07  3.74x    211319 |  56990  82735 |          0    128/4316   |     0/128/4316     0 | 8/4316

stream:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        451632 15.73  1.40x    564592 | 263547 183165 |          0      0/0      |     0/0/0     0 | 0/0
B        632355 22.03  1.00x    790430 | 376870 183858 |          0      0/0      | 36895/0/4096  7994 | 0/0
C-ID     205977  7.18  3.07x    257523 |  65598 107580 |          0   1024/4096   |     0/1024/4096     0 | 0/0
D-0      215278  7.50  2.94x    269152 |  65723 108563 |          0      0/0      | 36895/0/4096     0 | 1030/4096
D-ID     222082  7.74  2.85x    277767 |  65598 123685 |          0   1024/4096   |     0/1024/4096     0 | 1024/4096
```

Comparisons (warm window, `embedded`; `desktop` within a few percent):

| Comparison | calls | sort | stream | Reading |
| --- | ---: | ---: | ---: | --- |
| A → B (one shared DRAM) | 1.51× slower | 1.99× | 1.38× | little arbitration conflict (both ports requesting in 0.05–0.3 % of cycles); most likely DRAM row changes between interleaved instruction and data accesses (not counted, see below) |
| B → C-ID (L1 pair) | 3.58× faster | 3.93× | 3.06× | the code fits the I-L1 (no I-L1 burst in the window); the data L1 holds `sort`'s working set only partly (128 bursts) |
| B → D-0 (L2 alone) | 3.46× | 3.71× | 2.91× | a single-word L2 hit costs about an L1 hit, so the L2 alone recovers almost all of the gain |
| C-ID → D-ID (L2 under the L1s) | 5.7 % slower | 7.8 % slower | 8.3 % slower | negative: every store is written through two levels (D-L1, L2, DRAM) and the core waits for each write response; D-ID's data waits are 11,065 cycles above C-ID in `sort` (4,316 writes, about 2.6 cycles each), while serving the 128 D-L1 misses from the L2 saves about 770 |
| D-0 → D-ID (L1s over the L2) | 1.9 % slower | 1.7 % slower | 3.1 % slower | same write-through cost; the L1s add a level for stores |

The CPI floor with caches is about 5: one outstanding fetch per port and a
two-cycle L1 hit give at least two cycles per instruction from fetch alone,
plus load and store waits. The L2's value in this core is limited by
write-through without a write buffer; that is a property of the cache design
as built (write-through, no-write-allocate, one outstanding request), not
of the memory timing, and it is reported, not tuned.

**Standalone locality** (owner request; the same 2048 reads, cycles per
read; `embedded` / `desktop`):

| Access pattern | S0 DRAM | S1 L1 | S2 L1+L2 |
| --- | ---: | ---: | ---: |
| sequential (`stream`) | 8.05 / 8.03 | 5.08 / 5.26 | 5.83 / 6.07 |
| one word per line (`stride-line`) | 8.12 / 8.08 | 14.29 / 14.79 | 17.32 / 17.97 |
| one word per DRAM row (`stride-row`) | 19.93 / 21.94 | 26.20 / 29.31 | 29.19 / 32.46 |

Locality-friendly access is 2.5× faster with no cache at all (DRAM row
locality: 8 vs 20–22 cycles) and about 5× faster with an L1 (5.1 vs 26.2).
Caches make access with no spatial locality slower (one word per line: 14.3
with an L1 against 8.1 without), since every miss fetches four words to use
one.

**Gaps:** the panel does not count DRAM row changes or refreshes (the plan
asked for them), so the A → B row-change explanation is an inference from
Step 1 and the stride-row trace, not a measurement in the CPU runs. The
D-ID failure-point recheck is still to do.

## 2026-10-17 — Cache campaign: pointer-chase, clock recheck, plan check

`pointer-chase`, 512 nodes (4 KiB), all ten runs verified:

`embedded`:

```
pointer-chase:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        134203 13.06  1.85x    167864 |  98978  46178 |          0      0/0      |     0/0/0     0 | 0/0
B        248769 24.22  1.00x    311042 | 147656  81674 |          0      0/0      | 16415/0/0   861 | 0/0
C-ID      82295  8.01  3.02x    102981 |  24638  45338 |          0   2000/0      |     0/2000/0     0 | 0/0
D-0       75971  7.40  3.27x     95528 |  24817  34738 |          0      0/0      | 16415/0/0     0 | 1454/0
D-ID      79289  7.72  3.14x     99664 |  24638  42332 |          0   2000/0      |     0/2000/0     0 | 1440/0
```

`desktop`:

```
pointer-chase:
top      window   CPI   vs B     whole | I wait  D wait | IL1 bursts DL1 bursts/writes | ARB rd/burst/wr  both | L2 bursts/wr
A        125641 12.23  2.19x    157069 |  98874  33090 |          0      0/0      |     0/0/0     0 | 0/0
B        275375 26.81  1.00x    344320 | 155889  89867 |          0      0/0      | 16415/0/0  3145 | 0/0
C-ID      72584  7.07  3.79x     90804 |  24638  35627 |          0   2000/0      |     0/2000/0     0 | 0/0
D-0       69702  6.79  3.95x     87477 |  24825  28461 |          0      0/0      | 16415/0/0     0 | 1454/0
D-ID      73041  7.11  3.77x     91772 |  24638  36084 |          0   2000/0      |     0/2000/0     0 | 1440/0
```

- Random dependent loads miss the D-L1 on almost every node (2,000 burst
  fills for 2,048 nodes, one miss per two loads; `stream` misses once per
  four loads). The L2 alone beats the L1 pair here (D-0 75,971 against C-ID
  82,295 cycles at `embedded`; 69,702 against 72,584 at `desktop`): the
  256-B L1 rarely holds a node and each miss fetches a whole line, while the
  2-KiB L2 holds part of the 4-KiB array and answers a single word as fast
  as an L1 hit. Whole-program CPI is not a clean locality comparison with
  `stream` (different instruction mix; `stream` stores every word); the
  standalone stride traces are the direct evidence.
- `desktop` runs `pointer-chase` in fewer cycles than `embedded` with caches:
  its DRAM rows are 4 KiB, so the whole node array sits in one open row.

**Clock recheck for D-ID** (program 20): completes at `embedded` 3000 ps and
deadlocks at 2400; completes at `desktop` 2000 and deadlocks at 1600 — the
same points as C-ID. The campaign clocks (4104 / 2858 ps) keep at least
37 % / 43 % margin for every cached topology measured.

**Plan check after the cache campaign (assistant):**

- The campaign's completion rule is met: every declared run verified (30
  panel runs, 10 pointer-chase runs, the standalone traces in both
  profiles), and the B/C/D comparisons have explanations supported by the
  counters (L1 pair 3–4× over the shared uncached bus; L2 alone close to
  it; L2 under the L1s 2–8 % slower because of write-through through two
  levels; random access favours the larger L2). One explanation, A → B, is
  an inference (DRAM row changes are not counted); recorded as a gap.
- Results stay exploratory and unpublished until (1) the gate-level clocked
  macro defect inside the cache is explained (the campaign used compact
  banks), and (2) the clock for cached roots has stronger evidence than a
  measured failure point (the cache's state bits have no setup check).
- Not tuned: no memory interval, cache latency or policy was changed after
  seeing results. The write-through cost is a property of the cache as
  built; a write buffer or write-back cache would be a new, separately
  tested design, not a fix to these results.
- Next: phase 8, in this order: the macro defect; DRAM row/refresh counters
  for the experiment; the cached-root clock evidence; the test-timing audit
  and full suites; the visualizer; the public site only when the owner
  allows documentation updates.

## 2026-10-17 — Phase 8: gate-level clocked SRAM macro write disturb (found and fixed)

- **Localised.** Watching the 64 cells of row 3 in the cache's way-0 tag
  bank showed word 7 changing whenever word 4 was written. On the macro
  alone (`probe/macro_rw.cpp`): write word w, then word v, read w, for every
  pair. At `embedded` the gate-level macro disturbed four pairs (writing
  word 0, 1, 2 or 3 changed word 4, 5, 6 or 7, the previous access's row,
  same column); `unit` and the compact form, none. The earlier isolated
  tests had always written the disturbed word last.
- **Cause** (`probe/macro_trace.cpp`, 5-ps steps through the write of word 0
  after word 4, `embedded`): before the edge, the finished write of word 4
  still had WRITE_ENABLE = 1, ROW_ENABLE = 1 and word line 2 on — the clocked
  macro never ended its write phase. At +425 ps ROW_ENABLE fell, at +475 ps
  the held address and data switched to the new access, at +525 ps word
  line 2 closed: for 50 ps the still-enabled drivers wrote the new word into
  the old row. The clocked macro's settle window is the minimum (its timer
  starts on the access token), shorter than the word line's turn-off; the
  asynchronous macro has a long settle window and is not affected.
- **Fix** (`SRAM.tpp`, clocked macro only): the write drive ends once the
  write is through, WRITE_ENABLE = stage 1 · writing · ¬READY, as a real
  macro ends its write pulse. Trace after: WRITE_ENABLE is already 0 at the
  next access; the write keeps about 540 ps with its word line on.
- **Checks:** the disturbance test finds 0 pairs in `unit`, `embedded` and
  `desktop`; `SRAMClockedTest`, `SRAMClockedSubarrayTest`,
  `SRAMContractTest`, `SRAMTest` pass; the standalone cache benchmark with
  **gate-level banks** (`CACHE_BENCH_GATE_BANKS`, default profile,
  `embedded`, 7,606 s) completes all 24 traces verified, with cycle counts
  and traffic identical to the compact-bank run. The campaign's compact-bank
  results therefore agree with the lower-level form in the standalone
  benchmark. Tables are being regenerated and the delay rule rerun.

## 2026-10-17 — Phase 8: macro fix revised (replaces the write-pulse fix above)

- **The write-pulse fix was withdrawn before commit.** Its regeneration
  showed only the clocked SRAM device tables changing, but their latest
  READ_DATA launch grew by 640–730 ps at `embedded` (for example 1611 →
  2247 ps): the analysis followed CLK → READY → write enable off → bitlines
  → sense → READ_DATA, a path that cannot carry a change (a write disables
  the output) but has no state cut in the analysis. The compact macro would
  then show read data up to 0.7 ns late, cycles real hardware would not
  spend. The partial regeneration was stopped and the tables restored.
- **Revised fix:** the macro's settle line, which its own comment says must
  clear the previous access before the registered inputs reach the array, is
  now sized for that (`circuit::sram::clockedSettle(rows)`): the timer's
  detection path plus (decode depth + 3) slowest gate delays — the stage
  gate, the row decoder's enable path (enable AND and balanced tree), and the
  write phase AND and driver. The compact macro's withdrawal and the
  fallback access times use the same function. The write phase is unchanged
  (it stays on until the next access, as before).
- **Checks:** trace at `embedded`: the old write drive off at +475 ps, word
  line 2 closed at +575, the new address at the array at +785 (210 ps
  margin); every access now starts about 310 ps later (word line 0 at +1505
  instead of +1195). Disturbance test 0 pairs in all three profiles;
  `SRAMClockedTest`, `SRAMClockedSubarrayTest`, `SRAMContractTest`,
  `SRAMTest` pass. Tables regenerating; delay rule, the gate-level-bank
  cache benchmark and the affected clocks and cycle counts follow.
- **Results of the revised fix:** tables at a fixed point after round 2 and
  recomputed identically in all three profiles; only the clocked SRAM
  device's launches moved (READY and READ_DATA 260–364 ps later at
  `embedded`). DelayRuleTest on the SRAM device, controller and endpoint (9
  parts): no break, no target missed, in `unit`, `embedded` and `desktop`.
  CPU panel with on-chip SRAM unchanged (clocks 174 / 4104 / 2858 ps, every
  cycle count identical); `RV32IExperimentCLITest` passes. The standalone
  cache benchmark (compact banks) is identical to the campaign run in both
  profiles, so the campaign's cycle counts stand.
- **Cached roots' failure point moved:** C-ID completes at `embedded` 3300
  ps and deadlocks at 3000 (was 3000 / 2400); `desktop` 2300 / 2000 (was 2000
  / 1600). The campaign clocks are now 24 % above the shortest working
  period in both profiles (were 37 % and 43 %; an earlier line in this log
  called those figures margins "above the failure point", they were above
  the last working period).
- The gate-level-bank standalone benchmark is being rerun with this fix.

## 2026-10-18 — Phase 8: gate-level banks confirm the campaign; DRAM command counters

- **Gate-level banks, revised fix:** the standalone cache benchmark with
  gate-level banks (`CACHE_BENCH_GATE_BANKS`, `embedded`, 7,661 s) completes
  all 24 traces verified, with cycle counts and traffic identical to the
  compact banks. The campaign's standalone results are confirmed by the
  lower-level form.
- **DRAM command counters (instrumentation):** the compact DRAM control
  counts the commands it accepts (activate, read, write, precharge,
  refresh, read_burst); `BehavioralMemoryEndpoint::commandCounts()` reports
  them and the experiment adds `DEVICE_<memory>_<command>` per measured
  window. Observation units in the benchmark record are now labelled
  (requests, words, commands, cycles; all were "cycles").
- **A → B explained by measurement** (`sort`, `embedded`, same window as the
  panel, cycle counts unchanged by the counters): A opens DRAM rows 148
  times in the instruction memory (one per refresh: the code stays in one
  row) and 648 times in the data memory (input and output are in different
  2-KiB rows); B's shared DRAM opens rows 18,377 times — 23× A's 796 —
  because instruction fetches and data accesses interleave on one DRAM.
  At about 12 extra cycles per row change, the extra 17,600 activations
  account for roughly 211,000 of B's 278,700 extra cycles; most of the rest
  is the shared bus. This replaces the inference recorded with the panel
  results. Checks: `RV32IExperimentTest`, `RV32IExperimentCLITest`,
  `RV32ITopologyTest`, `MemoryEndpointDRAMTest`,
  `NativeMemoryDeviceContractTest` pass.

## 2026-10-18 — Phase 8: full suite under `unit`

`ctest -j1` on `build-cleanup` at `ebde0d9` (466 tests, 4,996 s): 459 pass,
7 fail.

| Failing test | Cause | Since |
| --- | --- | --- |
| `RegisterFileDocumentationTest` | asserts that `site-docs/memory/3.4.md` does not exist; the page has existed since the branch baseline | before this branch (documentation) |
| `VisualizerColorPaletteTest` | the palette has no entry for the cache's types and still lists retired ones (`MemoryContainer`, `CacheReadController`) | baseline era (visualizer data) |
| `VisualizerAutomaticLayoutTest`, `CacheLayoutTest` | saved layouts are stale (cache controller since 7a, arbiter since 7c) | this branch (visualizer data) |
| `MemoryEndpointIntegrationTest`, `MemoryDeviceDocumentationTest` | `site-docs/` snippets and pin tables | this branch (documentation) |
| `DelayRuleStrictSettlingTest` | `memory.rom.controller.32` and `memory.sram.controller.32`: under strict settling the compact REQ_READY changed 7 ticks after the clock edge, the gates 8 (tables identical across widths: REQ_READY launches 6–9) | not run in this branch before; investigated below |

The six documentation and visualizer failures belong to phase 8's
visualizer and site work (the site waits for the owner). The strict-settling
break is a delay-twin defect candidate: each twin flip-flop kept one launch
delay for both Q and Q_BAR, overwritten by whichever output the builder met
last, so paths from the other output can be timed a gate early. Changed to
a launch delay per output; rechecking.
- **Per-output launch delays: not the cause.** With each twin flip-flop
  given separate Q and Q_BAR launch delays, the strict check still broke
  both controllers the same way (7 against 8 ticks); the normal delay rule
  passed on them before and after. The change was reverted (not committed)
  so that only verified changes remain. The visualizer test run had written
  four entries into `visualizer-v2/layout.json`; restored (a test side
  effect, recorded for phase 8's visualizer work).
- **Strict settling: an open delay-twin defect, not a unit artefact.**
  Reproduced with a net-tracing copy of the test (`probe/dr_nets.cpp`): at
  a clock edge with REQ_OP = 011 (reserved), REQ_VALID = 1, the gates'
  sequencer DONE and LAST state bits switch 2 ticks after the edge, then
  COMPLETION (3), FINAL_COMPLETION (4), REQUEST_SLOT (5), the READY tree
  (6, 7) and the output pin (8); the compact form said 7. In `embedded`
  and `desktop` the same check breaks five controllers: REQ_READY of the
  32-bit ROM/SRAM controllers (305 against 386 ps embedded), RSP_VALID of
  the 8-bit ROM/SRAM and clocked SRAM controllers (277 against 286 ps). The
  twin's flip-flops launch at the contract's maximum clock-to-output (3–4
  ticks at `unit`, above the gates' 2), so the twin must be timing a shorter
  path downstream than the gates take; the cause is in the twin's settle
  evaluation for these controllers and needs its own session.
  Scope: the normal delay rule passes in all three profiles (the proven
  tables cover the gates), and the experiments use the fused endpoint, not
  compact controllers with twins, so no recorded result depends on it. It
  stays open in phase 8; the embedded full suite proceeds.

## 2026-10-18 — Phase 8: full suite under `embedded`; test-timing audit

`CIRCUITSIM_TIMING_PROFILE=embedded ctest -j1` at the same state (3,182 s):
249 pass, 217 fail. Nearly all fail within hundredths of a second, for one
reason: the tests are written in `unit` ticks. Two kinds:

- **Tick arithmetic** — the test asserts exact tick counts or certified
  periods ("Three inverters take three ticks"; "Expected safe period 10, got
  96"; timestamp checks in `FullCircuitTest`; pinned clocks in the CLI
  tests; the `unit` table file in `PartDelayTablesCompleteTest`). These are
  correct only in `unit` by design.
- **Fixed clocks** — a clock half period in ticks sized for `unit` (the CPU
  program harness: 50 ticks for the five-stage core, 2,000 for single-cycle
  and baseline; the cache and memory protocol rigs), far shorter than a gate
  chain at 1 ps per tick: programs compute nothing or never commit. About
  180 RV32I program tests (three cores, five predictors, 22 programs) and
  the visualizer tests that build programs are in this group.

**Plan revision (assistant).** The plan's decision 2 asked to derive every
test timing from the active profile and pass the full suite in both
profiles; phase 3 had already rejected rewriting hundreds of tick-based
scenarios. Now, with evidence:

1. The CPU program harness derives its clock from the profile:
   `RV32IProgramRoot` multiplies its `unit` half period by
   `timing::logicScale()` (the largest ratio, active profile against `unit`,
   over the logic gates and a flip-flop's clock-to-output and setup: 1,
   120 and 84 for `unit`, `embedded`, `desktop`). Same cycles, longer ticks;
   `unit` unchanged.
2. Tests of tick arithmetic are pinned to `unit` explicitly (per-test
   `CIRCUITSIM_TIMING_PROFILE=unit`), so a suite run under another profile
   still runs them, correctly.
3. Tick-clocked memory and cache protocol rigs are scaled the same way
   where cheap, otherwise pinned and named here.
The remaining failures after (1) decide the split; rerun in progress.
- **Harness scaling tried and withdrawn.** Multiplying the program
  harness's clock by the profile's logic scale (120 for `embedded`) fixed
  none of the 217: the CPU harnesses also step and sample in `unit` ticks
  (`cycle_time_step` 100 or 4,000, reset releases at 21 or 2,800, sampling
  20 ticks before each edge, a 100-tick observation origin), about 30
  constants across five harness files. A rerun after the reboot-like
  session interruption (the host stayed up; the session's jobs and one
  compile were killed; the build was resumed) confirmed it.
- **Decision, replacing the plan's decision 2 of 2026-10-17:** the native
  suite stays a `unit` suite; a whole-suite run under `embedded` is not a
  valid check and is dropped. Realistic profiles are covered by tests that
  set their own profile: new `RV32IProfileSweepTest` (single-cycle and
  baseline with FF memory, advanced with FF, on-chip SRAM and DRAM;
  programs 1, 9, 16, 19, 20; `embedded` and `desktop`; each at the
  analyzer's certified clock; oracle-verified; no setup violation: 50 runs,
  113 s, pass), `RV32ITopologyTest`, the per-profile table checks and the
  CLI checks. Reasons: the program tests under `embedded` would duplicate
  the phase 6.5 core sweep (every core, predictor and program at
  `embedded`, oracle-checked); phase 3 already kept tick-based tests on
  `unit`; and the remaining tick-arithmetic tests are only meaningful in
  `unit`. The harness change and its helper were reverted. The tests guide
  (`docs/guides/tests-and-scenarios.md`) now states this.

## 2026-10-18 — Phase 8: delay-twin X timing (strict-settling break explained)

- **Method:** a net-tracing copy of DelayRuleTest (`probe/dr_nets.cpp`)
  linked with a logging copy of `DelayTwin.cpp` (node names kept, settle
  times and flip-flop captures logged), on `memory.rom.controller.32` in
  the failing situation.
- **Finding:** at the failing clock edge the twin's flip-flops (PENDING,
  DONE, LAST, the response buffer's OCCUPIED) all held X, their data inputs
  X since long before; the gates' state was known (DONE and LAST went 0 → 1
  at 2 ticks). An earlier situation with X inputs had left the twin's state
  unknown while the gates resolved. A flip-flop whose value stayed X was
  treated as not changing (`held_since` renewed only when the held value
  changed), so every path from DONE and LAST looked quiet and the twin timed
  REQ_READY from the one flip-flop that visibly changed (OCCUPIED, 0 → X):
  final at 7 where the gates settled at 8. This is the open "delay twin's X
  timing" defect.
- **Fix** (`DelayTwin.cpp`): an unknown flip-flop may switch at any edge, so
  its outputs are timed from every edge (and from an unknown reset), not
  only when the held value changes. This only makes the twin later, which
  the delay rule allows.
- **Check:** DelayRuleTest `--strict-settling` on the five affected
  controllers (ROM/SRAM 32-bit and 8-bit, DRAM 32-bit, with variants) in
  `unit`, `embedded` and `desktop`: 0 parts break the rule, 0 miss the
  targets, 0 outputs moved only in the gates (were 2, 5 and 5 parts broken).
- **Full suite under `unit`** (467 tests with `RV32IProfileSweepTest`):
  461 pass in 10,678 s on a loaded host (load about 6 from other jobs;
  DelayRuleTest alone took 2,372 s). The six failures are the known
  documentation and visualizer-data checks (RegisterFileDocumentationTest,
  MemoryEndpointIntegrationTest, MemoryDeviceDocumentationTest,
  CacheLayoutTest, VisualizerAutomaticLayoutTest, VisualizerColorPaletteTest).
  DelayRuleTest and DelayRuleStrictSettlingTest both pass, so the twin
  change made no part's compact timing earlier than its gates.
- **Decision:** the delay-twin X-timing defect is closed. Phase 8 continues
  with the visualizer data and clock evidence for cached roots.

## 2026-10-18 — Phase 8: visualizer data

- **Failures before:** VisualizerColorPaletteTest (38 cache and memory
  timing layout types had no palette color), VisualizerAutomaticLayoutTest
  `test_selective_refresh` (a refresh of the SRAM scenario added layout
  types that were not saved: SymbolicDiffer, MemoryTimer<43,4>,
  MemoryDelayLine), CacheLayoutTest (the three cache scenarios' saved root
  layouts still named the bank helpers from before 7a, such as
  TAG_SELECT_ZERO; the cache now has TAG_0_READ_ENABLED and so on).
- **Changes to `visualizer-v2/layout.json`:**
  1. Removed type and profile entries of parts no longer in the source
     (CachePhaseSequencer, CacheReadController, MemoryContainer,
     MemoryNativeSequencer; 17 type entries, 8 profile entries) and five
     hashed cache variants (`Cache<8>[layout-…]`, `CacheRequestCheck<…>[…]`)
     whose child structure no scenario builds any more.
     `tools/prune-retired-layouts.py` was not used for this: it reads a
     `[layout-…]` suffix as part of the type name and flagged the live cache
     scenario roots, so it would have deleted live entries.
  2. The three cache scenario floorplans were rebuilt with
     `recalculate_layouts.py --fill-missing cache-read-hit cache-direct-mapped
     cache-scalable-two-way --reflow-types 'Cache<8>' 'Cache<32>'` (the
     cache's own floorplan, `cache_layout.py`).
  3. A full `--automatic-only` refresh ran on a copy (276 representative
     topologies, 1.83 M components, 6.5 min, 182 MB peak). It changed 60
     scopes: 51 had no saved children (new parts and new profile variants;
     these defaults were taken), and 9 would have moved saved children of
     parts this branch did not change (ZeroDetect8, the baseline fetch stage,
     the single-cycle next-PC and memory roots, ALU8ControlDecoder,
     ALU8ResultSelector). No visualizer code or those parts changed on the
     branch, so that drift predates it; those entries were left as they
     were. 37 new type and profile entries were added.
- **Palette** (`visualizer-v2/palette.json`): colors for the cache family
  (the cache, bank controller, lookup phases, request check, address path,
  tag match, line state, replacement state, fixture) and for the memory
  timing parts (static and command sequencers, request checks, timers, delay
  line, symbolic differ, protocol-stable data, known bits, power-on reset,
  synchronizer), in the hues of their existing families. The retired
  MemoryNativeSequencer prefix is replaced.
- **Check:** CTest label `visualizer` (29 tests): 28 pass; the failure is
  MemoryEndpointIntegrationTest's three `site-docs/` checks, unchanged.
  VisualizerColorPaletteTest, VisualizerAutomaticLayoutTest and
  CacheLayoutTest pass; `layout.json` was unchanged by the run. The browser
  and WebAssembly checks (`tools/run-web-tests.py`) were not run.
- **Remaining failures in the full suite** are only the documentation
  checks that read `site-docs/` (MemoryDeviceDocumentationTest,
  RegisterFileDocumentationTest, MemoryEndpointIntegrationTest), which wait
  for the owner's permission to update the public pages.

## 2026-10-18 — Phase 8: clock evidence for cached and shared roots

- **Correction** (replaces the 2026-10-17 statement that a cached root
  below its limit fails "with zero setup violations reported"): in the
  current build, C-ID program 20 at `embedded` 3300 ps completes and
  verifies but reports 389 setup violations (the first at
  `IL1.CONTROLLER.READ_RESULT` D, 163 ticks before the edge where 333 are
  needed); at 3000 ps it exceeds its cycle budget. The cache's registers are
  compact parts with proven tables, so their setup checks run. Why the
  earlier runs reported none was not re-established (they predate the
  clocked-macro fix and the later table regenerations).
- **Analyzer diagnostics for C-ID at 4104 ps** (`--safe-clock`): status
  unknown, and not because of memory contracts. Two kinds of message:
  (1) the arbiter's state bits are clocked by `SHARED_CLOCK`, an AND of
  I_CLK and D_CLK ("derived, gated, or different clock"); (2) each cache
  controller hands its clock to the banks through `CHILD_CLOCK`, a
  zero-delay one-bit Rewire ("clock used as data").
- **Decision 1 (design):** the arbiter's AND in the clock path is a design
  fault, not an analyzer limit: in simulation it delays the arbiter's edge
  by one gate delay behind its requesters (clock skew). The arbiter is now
  clocked by I_CLK; D_CLK must carry the same clock and is not read
  (header comment). The pins are unchanged (they follow the memory
  protocol's per-port CLK/RST); the arbiter has no public page and no saved
  layout.
- **Decision 2 (analyzer):** `SafeClockTiming` accepts a zero-delay Rewire
  with one input bit and one output bit, fed by the root clock (or by such
  a wire), as part of the clock net: its output is the clock with no skew.
  This is the rule the delay-table analyzer (`TimingGraph::clockSource`)
  already applies. The report states it as an assumption ("N zero-delay
  one-bit Rewire(s) pass the clock on unchanged"). Gates and any other part
  in the clock path stay refused.
- **Results** (`--safe-clock`, program 20, DRAM): every topology is
  `safe-under-contracts` with no diagnostics, at 4104 ps (`embedded`) and
  2858 ps (`desktop`). The core's own memory-port setup demands
  (`also_captured`, from the compact core's table) set that period in every
  topology, so no cache or arbiter path needs more than the core's clock.
  The analyzer gives a sufficient period, not a minimum; the cache's own
  shortest safe clock is not computed. Endpoints behind a cache or the
  arbiter are taken with all their arcs (no handshake-state cuts), which is
  conservative.
- **End to end:** `--period auto` (structural reference certified, then
  simulated) now works for cached roots: C-ID 232 cycles and D-ID 238
  cycles at 4104 ps, both verified, 0 setup violations (C-ID's 232 equals
  the 2026-10-17 run).
- **Campaign unchanged by the arbiter change:** B and D-ID `sort` (64
  words, `embedded`) rerun: identical window and whole-program cycles
  (559,942 / 699,939 and 153,511 / 192,759) and identical boundary counters;
  verified, 0 setup violations.
- **Tests:** SafeClockTimingTest gains a case with the clock through a chain
  of two Rewires (safe at the same period, the assumption stated);
  RV32ITopologyTest gains `test_cached_and_shared_roots_have_a_certified_clock`
  (B and D-ID). Run and passed: SafeClockTimingTest,
  MemoryEndpointSafeClockTest, RV32IExperimentTest, RV32IExperimentCLITest,
  RV32IProfileSweepTest, RV32ITopologyTest, TwoPortMemoryArbiterTest,
  CacheReadTest.
- **Consequence for the campaign's status:** the two conditions set on
  2026-10-17 for leaving "exploratory" are met: the gate-level macro defect
  is explained and fixed (compact and gate-level banks agree), and the
  cached roots' clock is certified by the analyzer rather than by a
  measured failure point. The results stay unpublished until the owner
  allows the public site to be updated.

## 2026-10-18 — Phase 8: STM32F7 system check (closed as a plausibility estimate)

- **Sources tried:** RM0385 (STM32F75x/74x reference manual) is the
  authority; its flash wait-state table is Table 5, p. 80 in Rev 9 (Table 4,
  p. 75 in Rev 2), but the PDF could not be read from this host (direct
  downloads are blocked; the web fetch of ST's FMC training PDF timed out).
  Secondary sources agree on 7 flash wait states at 216 MHz (8 CPU cycles
  per uncached flash access) for 2.7–3.6 V. For the FMC SDRAM controller:
  SDCLK at most HCLK/2, CAS latency 1–3 SDCLK (2 in most published F7
  set-ups), a read pipe delay of 0–2 HCLK cycles. No source gave an
  end-to-end SDRAM read latency in CPU cycles.
- **Estimate** (HCLK 216 MHz, SDCLK 108 MHz, CL 2, tRCD and tRP 2 SDCLK at
  about 18 ns): row hit ≈ command 2 + CAS 4 + read pipe 0–1 + return 1–2
  ≈ 7–9 HCLK; closed row + tRCD 4 ≈ 11–13; row conflict + tRP 4 ≈ 15–17.
- **Model at `embedded`** (4,104 ps ≈ 244 MHz, Step 1 of the cache plan):
  open row 8, closed 14–15, row change 20–22 cycles. The row hit is inside
  the estimate; closed and conflicting rows are 1–5 cycles above it, which
  matches the residual recorded in 6.4c (one synchronizer edge per DRAM
  command). On-chip flash has no counterpart in the model (the model's NOR
  is external, 34 cycles against a 120 ns tACC datasheet part).
- **Verdict:** plausible, with the DRAM overstatement already known and
  recorded; no change to the model. The check rests on secondary sources and
  arithmetic, not on the RM0385 tables, and is stated so.

## 2026-10-18 — Phase 8: visualizer timing profiles, findings and design

- **How the profile is held:** one active profile per process
  (`setActiveTimingProfile`). It is read when parts are built and, for some
  memory models, while they run (`SRAMBehavioral` access timing,
  `DRAMSyncControl`, `staticAccessTime`). The layout fingerprint
  (`BuildProfile::fingerprint`) does not include it, so saved layouts carry
  over between profiles. The native server is threaded (up to two
  sessions); the WebAssembly worker has one session.
- **Which scenarios work under `embedded` today** (the 2026-10-18 full
  `embedded` suite): 160 of the 163 visualizer component scenarios pass
  (they mostly use settled checkpoints, so the profile does not matter).
  Failing: `FullCircuitTest`, `CacheTwoLevelScenarioTest`,
  `RV32IBaselinePipelineCoreTest`. Every program scenario fails (175 of 176
  runs): the program fixtures hard-code clock half-periods (2000 single and
  baseline, 50 five-stage) and about 30 tick constants (clock start, reset
  release, first enable) chosen for `unit`.
- **Design:**
  1. A stored table of certified core periods per profile
     (`rv32i::certifiedCorePeriod(core, profile)`), checked against the
     safe-clock analysis like the delay tables (`--period auto` in
     `RV32IProfileSweepTest` must return the stored value).
  2. Program fixtures: under `unit` unchanged (recorded tables stay the
     same); under a realistic profile the clock period is the certified one
     and the fixture's tick constants scale by the same factor.
  3. The three failing component scenarios are investigated one by one.
  4. Native server: each session holds a timing profile; a module-level lock
     makes it the active profile for every build, step and scrub of that
     session. WebAssembly: a binding sets the profile before building.
     Browser: a profile choice beside the fidelity profile, timelines in ps,
     ns or µs when the profile's tick is 1 ps, and the profile shown with
     the scenario. Browser and WASM tests for both paths.

## 2026-10-18 — Phase 8: published-result reruns, fixtures and visualizer profiles (in progress)

- **Reruns of the published RV32I campaigns** (binary-safe clocks, structural
  reference check on every case, warmups 0 and one repetition, since modeled
  ticks do not depend on host time):
  - `embedded` finished and verified: 22 fixed programs × 7 targets (154),
    5 longer branch patterns × 7 (35), predictor capacity 8/16/32 (3), the 2
    ELF fixtures × 7 (14).
  - The generated-family and core-gap panels first stopped at the default
    100 M event budget: the structural reference makes far more events under
    realistic gate delays (inputs reach each gate at different times). No
    fault; rerun with 4 G events.
  - **Process correction:** the campaign driver fingerprints every tracked
    source file and the binary, and refuses to mark a run complete if either
    changes. Worktree edits made during the runs invalidated the runs in
    progress (`desktop programs` failed with "Source or binary changed").
    The remaining runs restarted from a frozen snapshot (`git archive HEAD`
    plus the built binary, `--source-snapshot`); `git diff c7935d8 HEAD`
    shows no C++ change, so the snapshot sources match the binary. The 11
    finished `embedded` steps stay valid. Running: `embedded` generated and
    core-gap; all `desktop` steps.
- **Visualizer timing profiles, implementation (not yet built):**
  - `analysis::CERTIFIED_CORE_PERIODS` (`ExperimentClock.hpp`): the
    certified FF-memory clock per core and profile; `rv32i_experiment
    --list-certified-periods`; `RV32IProfileSweepTest` checks every entry
    against `--period auto`.
  - `rv32iFixtureTimeScale(core, unit_cycle)`: program fixtures keep their
    `unit` times; under another profile every time (half-period, cycle step,
    clock start, reset release, sampling point) is multiplied by the smallest
    whole factor that makes the cycle at least the certified one (five-stage
    100 → 4,200 ps in `embedded`; single-cycle 4,000 → 8,000 ps; baseline
    stays 4,000 ps, above 3,354).
  - `CacheTwoLevelScenarioTest`: stimulus spacing in inverter delays;
    `FullCircuitTest`: all times multiplied by the profile's slowest gate.
    Both unchanged under `unit`.
  - Server: sessions carry a timing profile; `active_timing()` sets it under
    one lock while a session is built and simulated (the whole run happens
    then; scrubbing reads history) and restores it; sessions are keyed by
    scenario and profile; `timing` in the circuit, state and apply requests;
    the topology response includes the profile (name, description, tick
    length). New binding `timing_profile_info`. WebAssembly:
    `setTimingProfile`, `timingProfileJson`; the worker passes `timing`.
    Browser: a Timing selector (kept in the URL as `?timing=`), times shown
    in ps / ns / µs when the tick has a length, otherwise ticks.
  - **Build stopped:** the first build of these changes was stopped by Claude
    Code for low memory while compiling the Python module; nothing was
    committed. Decision: build the C++ targets first (test runner and
    experiment CLI) in the foreground, the Python module after the reruns.

## 2026-10-18 — Phase 8: program fixtures under every profile (verified)

- **Built** (C++ targets only: `SimulatorLib`, `SimulationTestRunner`,
  `rv32i_experiment`; foreground, one job, address space 3 GiB; the Python
  module waits for the reruns to end).
- **Baseline fixture correction:** it still failed under `embedded`
  (instruction 0: PC 0, expected 4) although its 4,000-tick cycle is above
  the certified 3,354 ps, and the core verified at 3,354 and 4,000 ps
  through the CLI. Cause: the fixture releases reset on a falling edge,
  half a cycle before its first working edge, so half a cycle (2,000 ps) is
  what the circuit has to settle from reset. Its scale is now computed on
  the half cycle (`rv32iFixtureTimeScale("baseline", 2000)`): 8,000 ps
  cycles in `embedded` and `desktop`, `unit` unchanged.
- **Checks:**
  - 179 tests (`/program-` scenarios of all cores and predictors,
    `FullCircuitTest`, `CacheTwoLevelScenarioTest`,
    `RV32IBaselinePipelineCoreTest`) under `embedded`: 179 / 179 (were
    1 / 179 on 2026-10-18 before the change); under `unit`: 179 / 179.
  - The baseline program and core scenarios under `desktop`: pass.
  - `RV32IProfileSweepTest` (new case: every stored certified period equals
    `--period auto` in `unit`, `embedded` and `desktop`): passed, 197 s.

## 2026-10-19 — Phase 8: rerun scope reduced (decision)

- **Measured pace:** with structural-reference checks, the generated-family
  panel advanced one case per ten minutes per profile (host: 4 cores, load
  11–12 from other users' work; each rerun process got about half a core).
  Generated (105 cases) and core-gap (161) would take about 40 hours per
  profile.
- **Decision:** the generated and core-gap panels run with the instruction
  oracle and final-memory checks only (the behavioral runs are seconds per
  case). The structural-reference check stays on the fixed programs (154
  per profile), the branch patterns (35), predictor capacity (3) and the ELF
  fixtures (14), and a spot check covers the generated families on the
  advanced two-bit core in `embedded` (15). Compact-versus-gate timing is
  covered independently by DelayRuleTest in every profile. The restated
  results will say which rows had a reference check.
- **Incident:** stopping the runs, `pkill -f` matched the calling shell (the
  script name appeared later in the same command; exit 144) and the edit in
  that command did not run. The runs were then stopped by process ID and the
  edit made in a separate command. Finished steps kept: `embedded` programs,
  branches, capacity, fixtures; `desktop` programs (frozen snapshot).

## 2026-10-19 — Phase 8: rerun results, first comparison

- **Done and complete (frozen snapshot or earlier valid runs):** `embedded`
  and `desktop` fixed programs (154 each, all structurally checked),
  generated families (105 each) and core-gap cases (161 each, oracle and
  memory checks); `embedded` branch patterns, capacity and fixtures.
  Running: `desktop` branch patterns, capacity, fixtures; then the
  `embedded` generated-family spot check with structural reference.
- **Cycles do not depend on the profile:** every one of the 420 FF cases
  has the same cycle count in `embedded` and `desktop`; only the clock
  differs (single 7,164 / 4,986 ps, baseline 3,354 / 2,334, advanced
  4,104 / 2,858).
- **Same cycles as published:** the longer branch patterns reproduce the
  published `unit` ticks exactly (cycles × 166 / 80 / 100) for taken,
  not-taken, alternating and bursts. The irregular row differed because the
  rerun used `--size 16`; the published row is reproduced exactly with
  `--size 8` (checked for all seven targets), so the published campaign used
  size 8 for that pattern. The irregular pattern is rerun at size 8 in both
  profiles.
- **Conclusions per profile** (fastest of single, baseline and
  advanced/two-bit; geometric mean of advanced/two-bit time over
  single-cycle time):

  | Panel | `unit` (published clocks) | `embedded` | `desktop` |
  | --- | --- | --- | --- |
  | Fixed programs (22, traps included) | advanced 11, single 10, baseline 1; 1.044 | advanced 13, single 8, baseline 1; 0.993 | advanced 13, single 8, baseline 1; 0.994 |
  | Generated families (15) | advanced 14, baseline 1; 0.707 | same; 0.672 | same; 0.673 |
  | Core-gap cases (23) | advanced 20, baseline 3; 0.690 | same; 0.656 | same; 0.657 |

  The advanced core gains about 5 % over single-cycle in the realistic
  profiles (its clock is relatively shorter: single/advanced 1.66 in `unit`,
  1.75 in `embedded`); the published orderings otherwise hold.

## 2026-10-19 — Phase 8: published-result reruns complete

Every rerun step finished and wrote COMPLETE (frozen snapshot of `61b2027`
sources with the binary built from `c7935d8`; the earlier valid `embedded`
programs, branches, capacity and fixtures runs kept). Checks per row:
instruction oracle and final memory always; structural reference on the
fixed programs (154 per profile), branch patterns (35), capacity (3),
fixtures (14) and the generated-family spot check (15, advanced two-bit,
`embedded`). All passed, zero setup violations (binary-safe certificates).

- **Irregular branch pattern at size 8** (the published setting): rerun in
  both profiles, structurally checked; its cycles × 166 / 80 / 100 equal the
  published `unit` row exactly.
- **Predictor capacity:** cycles 1,363 / 1,363 / 1,239 and next-PC misses
  67 / 67 / 5 for 8 / 16 / 32 entries, identical to the published values;
  modeled time 5,593.8 / 5,593.8 / 5,084.9 ns (`embedded`), 3,895.5 /
  3,895.5 / 3,541.1 ns (`desktop`).
- **Longer branch patterns, modeled time in ns** (64 iterations):

  | Target | taken | not-taken | alternating | bursts | irregular |
  | --- | ---: | ---: | ---: | ---: | ---: |
  | `embedded` Single | 3,281.1 | 3,739.6 | 3,510.4 | 3,510.4 | 3,496.0 |
  | `embedded` Baseline | 5,034.4 | 5,249.0 | 5,141.7 | 5,141.7 | 5,135.0 |
  | `embedded` Adv NT | 3,201.1 | 2,938.5 | 3,069.8 | 3,069.8 | 3,078.0 |
  | `embedded` Adv T | 2,166.9 | 2,954.9 | 2,560.9 | 2,560.9 | 2,536.3 |
  | `embedded` Adv BT | 2,692.2 | 2,429.6 | 2,560.9 | 2,560.9 | 2,569.1 |
  | `embedded` Adv 1b | 2,183.3 | 2,437.8 | 2,823.6 | 2,363.9 | 2,569.1 |
  | `embedded` Adv 2b | 2,199.7 | 2,446.0 | 2,577.3 | 2,429.6 | 2,569.1 |
  | `desktop` Single | 2,283.6 | 2,602.7 | 2,443.1 | 2,443.1 | 2,433.2 |
  | `desktop` Baseline | 3,503.3 | 3,652.7 | 3,578.0 | 3,578.0 | 3,573.4 |
  | `desktop` Adv NT | 2,229.2 | 2,046.3 | 2,137.8 | 2,137.8 | 2,143.5 |
  | `desktop` Adv T | 1,509.0 | 2,057.8 | 1,783.4 | 1,783.4 | 1,766.2 |
  | `desktop` Adv BT | 1,874.8 | 1,691.9 | 1,783.4 | 1,783.4 | 1,789.1 |
  | `desktop` Adv 1b | 1,520.5 | 1,697.7 | 1,966.3 | 1,646.2 | 1,789.1 |
  | `desktop` Adv 2b | 1,531.9 | 1,703.4 | 1,794.8 | 1,691.9 | 1,789.1 |

  The per-pattern winners among the advanced predictors are the published
  ones (cycles are unchanged). One ordering changes: in `unit` the
  single-cycle core beat advanced not-taken on the taken pattern (76,028 vs
  78,000 ticks); in `embedded` and `desktop` advanced not-taken is faster
  (3,201 vs 3,281 ns; 2,229 vs 2,284 ns), so every advanced predictor now
  beats single-cycle on every pattern.
- **ELF fixtures, modeled time in ns** (single / baseline / NT / T / BT /
  1b / 2b): `embedded` compiled.elf 458.5 / 556.8 / 352.9 / 385.8 / 352.9 /
  361.2 / 361.2, self-check.elf 136.1 / 187.8 / 106.7 / 131.3 / 106.7 /
  106.7 / 106.7; `desktop` compiled.elf 319.1 / 387.4 / 245.8 / 268.7 /
  245.8 / 251.5 / 251.5, self-check.elf 94.7 / 130.7 / 74.3 / 91.5 / 74.3 /
  74.3 / 74.3.
- **Not rerun:** the memory-endpoint follow-up and the program-20
  default-policy pairs with timed memories (the phase 6.5 sweep covers the
  advanced core with every memory, oracle-checked, in `embedded`), and the
  Embench projection (a projection, not a measurement). Raw outputs are in
  the session scratch directory; the tables above are the record.
- **Plan check:** the reruns change no published ordering except the
  fixed-program win count (advanced 11 → 13 of 22) and add about 5 % for
  the advanced core over single-cycle. Nothing in the timing plan needs to
  change; the results wait for the documentation phase.

## 2026-10-19 — Phase 8: visualizer timing profiles (native verified)

- **Python module built** with a lower address-space cap (2.5 GiB, one job)
  on the owner's instruction to proceed without waiting for memory; the
  compile of `ModulesBinding.cpp` peaked at about 1.7 GB and finished.
- **New test `VisualizerTimingProfileTest`** (labels visualizer, system,
  timing): profile names and tick lengths; unknown profile refused; the same
  scenario under `unit` and `embedded` has the same checkpoints, the
  `embedded` one later times and a 1 ps tick; a program scenario
  (`rv32i-program1`) builds and records under `embedded`; the library's
  profile is back to `unit` after each build; sessions are kept per
  scenario and profile. Passed.
- **`VisualizerReferenceProfileTest`** (Node, runs the worker's real
  `loadScenario` against a stand-in bridge) needed the two new bridge
  functions; it now also checks that the profile is set before building and
  reported with the payload. Passed.
- **Visualizer group** (CTest label `visualizer`, 30 tests): 28 passed
  before the Node test fix; the remaining failure is the known
  `MemoryEndpointIntegrationTest` (`site-docs/`). `layout.json` unchanged.
- **Pending:** the WebAssembly build and the browser checks
  (`tools/build-pages.sh`, `tools/run-web-tests.py`), running now.

## 2026-10-19 — Phase 8: WebAssembly and browser checks for the timing selector

- **Pages build** (`tools/build-pages.sh`, Emscripten from
  `~/.cache/circuitsim-emsdk`, MkDocs from `/home/arnold/venv`): the
  WebAssembly module rebuilt; example rows regenerated from every scenario;
  strict MkDocs build passed. A first attempt failed in the layout
  validator because the address-space cap put on the whole script also
  limited Node, and WebAssembly reserves a large virtual block at start;
  the packaging step was rerun without the cap.
- **New browser check `VisualizerTimingProfileBrowserTest`** (suite
  browser): `?timing=embedded` selects the profile, the worker reports a
  1 ps tick, the time label reads ns, not ticks; switching to `unit`
  reloads, removes `timing=` from the URL and shows ticks. Passed.
- **Smoke suite, first run:** 11 of 13 passed. Two failures, both fixed:
  1. `VisualizerWasmBrowserTest`: in the compact (embedded) layout the new
     Timing label and selector added rows to the 32 px scenario row and
     pushed the scenario selector off screen. The compact view now hides the
     Timing label and puts the selector beside the scenario selector.
  2. `MemoryEndpointBrowserTest`: the static site stopped on
     "Missing layout for profile_layouts.d57a5ed1eeab12fc.MemoryController<ff,8>.children.RUNNING".
     The FF controller's gate-level form has new children (RUNNING, READY,
     NOTHING_HELD) that its saved profile layout lacked; the native server
     fills such gaps when it builds a session, the static site cannot. The
     2026-10-18 layout refresh had added only whole missing entries, not
     missing children inside existing ones.
- **New check `tools/check-static-layouts.py`** (CTest
  `VisualizerStaticLayoutTest`, labels visualizer, layout): replays the
  browser's layout lookup (`getLayoutFor` / `getChildLayout`) on
  `layout.json` for every component of every visualizable scenario
  (topologies only). Found 13 missing placements in 5 of 339 scenarios
  (the FF controller in two scopes, the memory-controller root,
  `MemoryTimer<33,3>` in the ROM scenarios, the two-level cache's
  `Cache<8>` variant). Filled by building those five scenarios once through
  the native server (`_ensure_child_defaults` adds only missing children):
  only additions, no saved placement moved; the server also raised
  `ROMAddressControl`'s aspect ratio 0.725 → 0.84 so its pins fit (its
  gate-level form has more pins). Recheck: 0 missing.
- **Reruns after the fixes** (one at a time, load watched):
  `VisualizerWasmBrowserTest`, `VisualizerTimingProfileBrowserTest`,
  `MemoryEndpointBrowserTest`: all passed, load about 4–5.
- **Incident:** the session ended while the endpoint check was hanging on
  the missing layout (the host's load average had reached about 136 over
  15 minutes; no reboot). Nothing was left running.

## 2026-10-19 — Phase 8: final full native suite

- **Run:** `ctest --test-dir build-cleanup -j1` at `dff56d5` (isolated
  build, `unit` profile, address space capped at 3.5 GiB), 8,741 s.
- **Result:** 467 of 470 passed. The three failures are the expected
  documentation checks that read the old `site-docs/` memory pages, which
  wait for the memory chapter rewrite:
  `RegisterFileDocumentationTest` (asserts `site-docs/memory/3.4.md` does
  not exist; the cache page is there since the cache work),
  `MemoryEndpointIntegrationTest` (the published pin tables and two C++
  listings) and `MemoryDeviceDocumentationTest` (ROM and ROM address
  control pin tables, the C++ listings, the page numbering).
- `layout.json` was unchanged by the run (guard copy compared).

## 2026-10-19 — Phase 8: browser smoke suite, one check at a time

- **Two session losses explained.** Both happened while
  `MemoryEndpointBrowserTest` ran. The host's 4 GiB swap was full (3.9–4.0
  GiB used; a code-server file watcher and two `mkdocs serve` processes of
  the owner hold about 2 GiB), so Chrome's growth pushed the machine into
  swapping; the check then hung to its 1,200 s timeout (exit 124) and the
  session ended with it. A first serial attempt also failed in 1 s per check
  because the PATH set for the Pages build put a Python without Playwright
  first; the checks were rerun with the system Python.
- **Safeguard (decision):** each browser check now runs in its own systemd
  user scope with `MemoryMax=2G` and `MemorySwapMax=0`, a 600–900 s
  watchdog, and memory and load sampled every 15 s; a load gate (< 8) runs
  between checks. Chrome's peak was 1.1–1.8 GiB; nothing reached the cap.
- **Result:** all 13 smoke checks pass on the site built at `dff56d5`
  (layouts, `app.js`, `styles.css`, `index.html`, `wasm_worker.js` compared
  with the sources): `FFMemoryBrowserTest`,
  `MemoryDocumentationBrowserTest` (uncapped, before the incident),
  `MemoryEndpointBrowserTest` (140 s capped; 1,229 s timeout uncapped),
  `RV32IBuildingBlocksBrowserTest`, `RV32IDocumentationBrowserTest`,
  `RV32IMemoryBrowserTest`, `RV32ISingleCycleDocumentationBrowserTest`,
  `SRAMDocumentationBrowserTest`, `SRAMLayoutBrowserTest`,
  `VisualizerScenarioBrowserTest`, `VisualizerWasmBrowserTest`,
  `VisualizerWasmRuntimeTest`, `ALU8WasmRuntimeSmoke`.
- **Testing phase complete.** With the full native suite (467 of 470, the
  three known `site-docs` checks) the documentation phase starts, as the
  owner scheduled it.
