# Memory timing realism review

Started: 2026-10-16. Status: closed. Code state: `alpha` at `1d861c0` with the
uncommitted cache worktree; probes linked `build-cleanup/libSimulatorLib.a`
built 2026-10-15 from that worktree.

## Summary

The owner asked whether the memory technologies' structural forms, and the
behavioral forms that copy them, are timed realistically, after the
[cache plan review](2026-10-14-cache-system-plan.md) found an SRAM cache hit
slower than a DRAM read. Finding: a memory access takes 288–896 ticks, but
the gates and array primitives in every structural device settle in at most
about 20 ticks. Almost all of an access is **fixed C++ timers**: device phase
timers (`phase_ticks`, 32) and controller guards (`guard_ticks`, 64), set in
the first memory commit (`c003ed7`, 2026-09-19) and never calibrated. The
"structural" controller for SRAM/DRAM/ROM/NOR flash times everything in one
C++ primitive that shares code with the behavioral form. No delay depends on
array size. The resulting speed order is inverted against real devices:
DRAM open-row and NOR-flash reads are faster than SRAM; ROM waits like SRAM
although its structure is combinational; a NOR block erase costs about one
SRAM read. Structural and behavioral forms agree exactly, so this is a
calibration problem, not an equivalence bug. Changing gate delays would not
fix it. A follow-up depth review found the arrays and periphery well built
and FF complete down to NAND gates, but every other technology's control
logic, and the delayed controller's sequencer, is ordinary digital logic
hidden in one C++ part. Recommended: rebuild that control as gates, choosing
synchronous cycle counting or self-timed delay chains, together with the
calibration choice. Next step: owner decisions; no code has been changed.

## 2026-10-16 — Where memory access time comes from

### Atomic delays

| Source | Delay | Where |
| --- | --- | --- |
| NOT/AND/NAND/OR/NOR/XOR gates | 1 tick each, independent of type and fan-out | `src/modules/basic/Gate.cpp` (`BEGIN_BASIC_PINS(..., 1)`) |
| `BitSplitter`, `Mux`, `Decoder` primitives | 1 | `include/modules/utility/BitAdapter.tpp`, `src/modules/basic/` |
| `BitJoiner`, `Rewire`, constants, clock | 0 | same |
| All memory `Primitive`s: SRAM cell access, bitline pair, sense amplifier, DRAM cell/sense column, `StaticReadColumn`, `MemoryOutputBuffer`, `ROMAddressControl` | 1, independent of rows/columns | `include/modules/memory/DigitalMemory.hpp:44`, `src/modules/memory/SRAM.cpp` |
| SRAM access timer (`AccessSequence`) | 4 phases × `phase_ticks` | `src/modules/memory/SRAM.cpp:122` |
| DRAM command timer (`Commands`) | ACT 3P, READ 1P, WRITE 3P, PRECHARGE 3P, REFRESH 5P | `src/modules/memory/DRAM.cpp:86` |
| NOR flash timer | READ 1P; PROGRAM `program_ticks`+2P; ERASE `erase_ticks`+2P | `src/modules/memory/NORFlash.cpp:88` |
| Controller sequencer (`NativeSequence`) | guard G before and after each native command strobe; SRAM/ROM wait 4P+2G; release G; result shown at the next clock edge | `src/modules/memory/MemoryEndpointEngine.cpp:43–170` |

Defaults and minimums: P=`phase_ticks`=32 (minimum 32), G=`guard_ticks`=64
(minimum 64), `program_ticks`=128, `erase_ticks`=256. Git history shows the
minimums arrived with `c003ed7`; the only later change (`9a880f7`) exempted
FF. No record or archived report gives a calibration basis; the public pages
call them "deliberately conservative simulator units, not calibrated
nanoseconds" (`site-docs/memory/3.1.2.md`).

### Structural paths are short

Proven gate-level paths from `src/components/selection/ProvenPartDelays.generated.inc`
(W=32, ticks, min/max):

| Device | Data path | Notes |
| --- | --- | --- |
| FF, 64 / 128 words | `WORD_ADDR`→`READ_DATA` 4–9 / 4–17 | Decoder, cells and read mux, all gates; no timer |
| SRAM | inputs→`READ_DATA` 1–18 (128 words 1–20) | Measured after the access timer releases sensing |
| DRAM | inputs→`READ_DATA` 2–15 | After the command timer |
| ROM | `WORD_ADDR`→`READ_DATA` 2–14 (128 words 2–16) | Fully combinational; no timer |
| NOR flash | inputs→`READ_DATA` 2–16 | After the command timer |

### Measured end-to-end reads

A scratch C++ probe (not in the repository) drove one 64-word W=32 endpoint
per technology, 16 words per row, refresh disabled, at clock periods 16 and
32, and counted ticks from request acceptance to response transfer. Results
at period 16 (period-32 values were exactly 16 higher, i.e. edge rounding):

| Technology | Read | Ticks | From source: device work + guards |
| --- | --- | ---: | --- |
| SRAM | any | 336 | 4P=128 work; 2G wait padding + G release = 192 |
| ROM | any | 336 | about 16 work; the rest is the SRAM wait copied |
| NOR flash | any | 304 | P=32 work; 4G = 256 |
| DRAM | open row | 304 | P=32 work; 4G = 256 |
| DRAM | closed row | 592 | ACT 3P + READ P = 128; 7G = 448 |
| DRAM | row conflict | 880 | PRE 3P + ACT 3P + READ P = 224; 10G = 640 |
| DRAM | write, open row | 368 | 3P = 96; 4G = 256 |
| SRAM | write | 336 | as read |

The fused behavioral endpoint and the `MemoryEndpoint` assembled through the
default catalog profile (host time 0.3 s versus 0.001 s, so built from parts)
gave identical ticks for every case. The cache plan's period-102 probe also
found identical CPU-clock cycles for both DRAM forms.

### Findings

1. **Time comes from constants, not structure.** Of a 288–880-tick access,
   the structural gates and array primitives contribute at most about 20
   ticks. The rest is P-based device timers and G-based controller guards.
2. **Guards dominate.** A DRAM open-row read is 32 ticks of device work and
   256 of guards; an SRAM read is 128 of work and 192 of guards. G=64 is
   about three times the longest proven settling path (about 20 ticks), and is
   charged at every native command strobe, so commands with more strobes
   (DRAM conflict: three commands) pay it repeatedly.
3. **The delayed "structural" controller is not structural in its timing.**
   For SRAM/DRAM/ROM/NOR flash, `MemoryController::buildInternals` builds a
   gate-level transaction port plus one `NativeSequencer` primitive that runs
   the same `NativeSequence` C++ code as the behavioral controller
   (`src/modules/memory/MemoryController.cpp:68,284`). So controller timing has
   no lower-level source of truth; behavioral/structural agreement is by
   shared code. Only the FF controller is gates (`MemoryFFController.hpp`).
4. **The speed order is inverted relative to real parts.**

   | Order in this model, fastest first | Typical real order, approximate |
   | --- | --- |
   | FF (gates, about 9–17) | flip-flop/register file |
   | DRAM open-row read = NOR-flash read (288) | on-chip SRAM cache, about 1–4 ns |
   | SRAM = ROM (320) | discrete asynchronous SRAM, about 10 ns |
   | DRAM write (352) | DRAM row hit at the chip, about 14 ns; random access from a CPU through a controller, about 50–100 ns |
   | DRAM closed row (576), conflict (864) | NOR-flash random read, about 50–100 ns |
   | NOR program (about 128+2P+guards), erase (about 256+2P+guards) | NOR program microseconds per word; block erase tenths of a second |

   The right-hand figures are general orders of magnitude, not sourced
   project measurements; they must be cited from named datasheets before
   any are adopted. The Infineon CY7C1069 asynchronous SRAM cited in the cache
   record has a 10 ns access.
5. **ROM copies SRAM's wait.** Its device is combinational (about 14–16
   ticks) yet the controller waits `4P+2G` as for SRAM. Its address decode is
   itself one C++ primitive (`ROMAddressControl`), not gates.
6. **DRAM command ratios.** READ:ACT:PRECHARGE is 1:3:3 phases. In DDR parts
   the column latency, activate-to-read and precharge times are of similar
   size (for example equal CL/tRCD/tRP grades). The model therefore makes
   open-row reads unusually cheap, which a 16-word row exposes. There is also
   no controller/bus latency, one bank, and no burst.
7. **NOR flash.** A read equals a DRAM open-row read and is faster than
   SRAM; program and erase are a few hundred ticks, roughly one SRAM access.
   Real program and erase are thousands to millions of times a read.
   Very long values would cost simulation time, so a choice is needed.
8. **No size dependence.** Wordlines are zero-delay wires; bitline, sense and
   read-column primitives take 1 tick at any row count; device timers are
   fixed P regardless of geometry. Only decoder/mux depth grows (about
   log2). In real arrays, wordline and bitline loading make access time grow
   with size; that is why small caches are faster than large memories. This
   model cannot show that effect at all.
9. **Large FF memories are a log extrapolation.** Delay tables above 128
   words continue the 64→128 growth per doubling: 32-bit FF read 9→17 ticks
   becomes about 89 ticks at 65,536 words, so a 256-KiB FF memory is
   zero-wait at the RV32I period of 100. The tables label this ESTIMATE; it
   is not physically plausible and underlies the published FF baseline.
10. **Gate delays are uniform.** AND (in CMOS usually NAND plus inverter) and
    XOR take the same 1 tick as NAND; fan-out and wire load are ignored.
    This is a deliberate digital abstraction. It affects every circuit's
    critical path, but contributes at most about 20 ticks to a memory access,
    so changing it would not repair the memory ordering.
11. **Row-size default.** `EndpointConfig::words_per_row` defaults to 2; the
    published DRAM results use it, so reads rarely hit an open row there.
    The cache plan's 16-word rows make the 1P read common.

### Should atomic BasicComponent delays change?

- **Gates:** not as the memory fix. Making AND/OR/XOR slower than NAND/NOR,
  or adding fan-out cost, would be a separate project-wide change
  regenerating every delay table and clock and invalidating published cycle
  and period results.
- **Memory array primitives:** this is where physical size belongs (bitline
  and wordline length). But the timers currently reserve fixed time, so a
  slower primitive would either be hidden inside P or break the timed
  sequence. It helps only if the device timing is *derived from* those
  primitives, for example by an SRAM replica (dummy) bitline whose delay
  grows with row count and fires the sense enable, a standard self-timing
  method, and a DRAM sense time that grows with bitline length.
- **Timers and guards:** the most direct lever, but they are C++ constants;
  any new values need a stated reference and the same change in the
  structural and behavioral forms.

### Options for the owner (none chosen)

0. Choose a reference: what one tick and one CPU period represent (for
   example a small embedded system with on-chip SRAM and external SDRAM and
   NOR flash), and source target ratios from named datasheets.
1. ROM: wait for the proven combinational path instead of the SRAM sequence.
2. Guards: derive the minimum from the proven settling bound of the attached
   device instead of a flat 64.
3. DRAM: bring READ closer to ACT/PRECHARGE; optionally add a fixed
   controller/bus latency, built as clocked stages.
4. SRAM: replace the fixed four-phase timer by geometry-dependent
   self-timing (replica bitline) so small arrays are faster than large ones.
5. NOR flash: read slower than SRAM; program/erase longer within a stated
   simulation budget.
6. Delayed-controller sequencing as gates (lower-level-first), so controller
   timing has a structural source. Large work; optional.
7. Large FF memories: add a size term or stop presenting them as realistic
   zero-wait memory.

Any change requires the structural and behavioral forms together,
regenerated `part_delay_tables`, `DelayRuleTest`, safe-clock recertification,
and reruns of the published timed-memory panel and §4.7 results, which would
become historical. The cache experiment should wait for this decision.

Verification this date: read the gate, primitive, SRAM, DRAM, ROM, NOR-flash,
controller, endpoint engine and delay-table sources and the public memory
pages; traced the constants' history in Git; ran two scratch probes (cache
latency at periods 50/102/400; per-technology reads at periods 16/32). No
repository code, test or constant changed. No CTest, delay-table generation
or full suite was run. The real-device figures above are unsourced
approximations.

## 2026-10-16 — Structural depth review of each technology and controller

The owner asked whether each technology's structural form, including its
controller, is well built and whether it should go deeper. Reviewed the
current worktree. A concurrent session is retiring `MemoryContainer`
(`docs/records/2026-10-16-native-memory-device-retirement.md`); its edits to
these files rename selection paths and display types and do not change any
device or controller structure.

### Method

Each structural part was classified as **gates** (built from the project's
gates, latches, flip-flops, registers, muxes and decoders), a **symbolic
primitive** standing for analog or electrical behavior (a C++ part with no
practical gate equivalent), or **hidden digital logic** (a C++ part doing
ordinary digital work: latching, decoding, counting, state machines,
checks or timing).

### Per technology

| Technology | Gates | Symbolic primitives (reasonable floor) | Hidden digital logic |
| --- | --- | --- | --- |
| FF | Everything: `DFlipFlop` from two NAND gated D latches, `FlipFlopCell` (mux + DFF), words, bank, per-word address decoder, access control, read selector | `RequestKnownBits` (X/Z detection; simulator metadata) | None |
| SRAM | Cell latch (`SRLatch`), rows, row decoder, precharge distribution, write drivers, column selector, bank wiring | Cell access transistors (`SRAMCellAccess`), bitline pair, sense amplifier, output buffer | `SRAMAccessControl`: address/data/mask holding latches, change detection, the four-phase sequence, output-enable logic; delay 0 |
| DRAM | Rows, shared row decoder, column mux, write distributor, bank wiring | `DRAMCell` (charge, destructive read, retention), `DRAMSenseColumn` (row-buffer bit and restore), output buffer | `DRAMCommandControl`: command decoder, legality checks, BUSY/ERROR/ROW_OPEN state, row/column/data/mask latches, refresh-row counter, phase timing |
| ROM | Cells (constants), rows, shared row decoder, column mux | `StaticReadColumn` (selected-row bitline), output buffer | `ROMAddressControl`: row/column field split, range check, enables |
| NOR flash | Rows, shared row decoder, column mux, write distributor | `NORFlashCell` (program 1→0, erase to 1), read column, output buffer | `FlashControl`: write-enable latch, command decoder, legality checks, block-erase row decoder (computed per row in C++), program/erase timing |

### Controllers

- **FF controller** (`MemoryFFController.hpp`): gates throughout — request
  check, status priority chain, response buffer, write/read strobes.
- **Delayed controller** (SRAM, DRAM, ROM, NOR flash,
  `MemoryController.cpp`): the transaction port and response buffer are
  gates. The **`NativeSequencer`** is one C++ primitive running the same
  `NativeSequence` as the behavioral controller. It holds request validation
  (alignment, range, opcode and mask checks that already exist as gates in
  the FF controller and in `CacheRequestCheck`), the per-technology command
  list, DRAM open-row register and row comparison, the refresh timer and
  insertion, all guards and phase waits, result/status capture and the
  clock-edge handoff.
- **Consequence for equivalence.** `DelayRuleTest` compares a self-timed
  part with "its gates only" (`tests/cpp/DelayRuleTest.cpp:354`), and the
  delayed controllers and endpoints are recorded exceptions there. Because
  the gate form contains the same C++ sequencer, controller timing is
  effectively compared with itself.

### Assessment

1. **Arrays and periphery are well built.** All five technologies use a real
   matrix organization: cells sharing wordlines, per-column bitline or sense
   parts, gate-level row decoders, column multiplexers, write distributors and
   output enables, reused across technologies where the job is the same.
   SRAM's precharge/release/access/sense split and DRAM's destructive read
   with restore are textbook-faithful. Representing transistor and charge
   behavior by small symbolic primitives is the right floor for a
   four-state digital simulator; going below it would need analog modeling.
2. **FF memory is complete down to NAND gates.** Its only simplification is
   a one-dimensional per-word decoder rather than a row/column matrix, which
   is appropriate within the 4,096-cell bound.
3. **Every non-FF technology hides its control logic in one C++ part.** These
   are ordinary digital circuits — latches, decoders, counters, state
   machines, comparators — of the kind the project builds elsewhere from
   gates (the RV32I control units, `TwoPortMemoryArbiter`, and the cache's
   one-hot `CachePhaseSequencer`). This falls short of the lower-level-first
   rule in `AGENTS.md`: the behavioral form shares the code instead of being
   checked against a lower-level circuit.
4. **The delayed controller is the largest gap.** It is shared by four
   technologies, decides most of each access's duration (see the previous
   section), and has no structural form for its timing or DRAM row policy.
5. **Timing is where deeper structure would matter most.** There are two
   lower-level ways to produce the waits that the C++ timers now produce:
   - **synchronous counting**: a gate-level controller state machine counts
     clock cycles per step, as real SDRAM controllers do for activate, read
     and precharge timing; waits then scale with the clock period;
   - **self-timed delay chains**: a chain of N gates gives N ticks of delay,
     as real asynchronous SRAM uses address-transition detection,
     pulse generators and a replica (dummy) bitline. A replica bitline whose
     length follows the row count would make small arrays faster.
   Either makes timing come from structure with existing 1-tick gates, so
   no atomic delay needs to change. A 32-tick phase is about 32 gates per
   device, not per cell.
6. **Smaller points.** `SRAMAccessControl` has delay 0 while other memory
   primitives have delay 1. Several parts use an AND gate with constant 1 as
   a buffer only to drive a hierarchy pin (SRAM precharge, write driver),
   adding one tick. Fan-out is free, so one precharge gate drives every
   column.

### Recommendation and order (owner decision)

Yes, go deeper — for the hidden digital control, not for the symbolic
array primitives. Suggested order, each with its behavioral twin checked
against the new gates:

1. Delayed controller as gates: reuse the gate-level request check, a
   per-technology one-hot command state machine, DRAM open-row register and
   comparator, refresh counter, response capture. Decide synchronous
   cycle counting versus self-timed waits first.
2. `DRAMCommandControl` as gates: command decoder, state bits, latches,
   refresh-row counter, phase generation.
3. `SRAMAccessControl` as gates: input latches, change detection and a
   delay-chain phase generator; then optionally a replica bitline.
4. `FlashControl` and `ROMAddressControl` as gates (small).

Costs: more gates per device (control only, small next to the arrays),
updated layouts and public component pages, removal of the self-timed
delay-rule exceptions, regenerated delay tables and clocks, and reruns of
published memory results if timing changes. Doing this together with the
calibration choice in the previous section avoids paying those reruns twice.

Verification this date: source reading only (memory device, controller,
endpoint engine, delay-rule test and catalog files in the current worktree).
No code changed; no build, CTest or full suite run.

## 2026-10-16 — What the delayed controller does and whether it is needed

The owner asked what the controller does and whether it is necessary.
Source: `NativeSequence::step` in `src/modules/memory/MemoryEndpointEngine.cpp`
and `MemoryController::buildInternals` in `src/modules/memory/MemoryController.cpp`.

| Job | Needed? |
| --- | --- |
| Present one common clocked REQ/RSP port (VALID/READY, byte address, mask, status) for every technology | Yes, while CPUs, the arbiter and caches share one port contract |
| Check alignment, range, opcode, mask and technology permission | Yes; already exists as gates in the FF controller and `CacheRequestCheck` |
| Latch the request and hold the response under backpressure | Yes; the port and response buffer are already gates |
| Translate to native pins: SRAM/ROM CE/WE/OE; DRAM PRECHARGE/ACTIVATE/READ/WRITE; NOR write-enable then PROGRAM/ERASE | Yes; this is the controller's core job |
| DRAM open-row tracking, row-change sequencing and refresh insertion | Yes; a real DRAM always needs this |
| Wait for completion: SRAM/ROM fixed `4P+2G`; DRAM/NOR fixed `kP+G`, then poll `BUSY` | Partly redundant: DRAM/NOR devices already report `BUSY`, so the fixed wait duplicates the device's own timing; ROM is combinational and needs no SRAM wait; the extra guards exceed proven settling |
| Hand the result to the clock edge | Yes for a clocked port |

Inside `Cache<W>` each tag and data bank is a full endpoint, so every bank
access pays the controller's handshake, guards and edge handoff
(`Cache.cpp:829–836`). A cache drives its own small arrays directly in real
designs; the controller is not needed there and causes much of the measured
hit cost.

All of these jobs are digital: registers, comparators, counters and a state
machine. The only non-obvious part is waiting a number of ticks that is
independent of the clock; a gate form would either count clock cycles (as
real memory controllers do) or use a gate delay chain. Conclusion: keep a
controller at memory endpoints and build it from gates; remove it from inside
the cache by letting the cache drive bank devices directly; drop the
duplicated fixed waits where the device reports completion. Source reading
only; no code changed or run.

## 2026-10-16 — Owner decision; record closed

The owner decided to act on this review: give all leaf parts realistic
delays, keep behavioral parts derived from their structural forms, and
rebuild the hidden memory control logic as gates. The work continues in the
[realistic leaf delays and gate-level memory control plan](2026-10-16-realistic-timing-plan.md).
This record is closed and frozen.
