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

Current phase: 4 (gate-level memory control).

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
