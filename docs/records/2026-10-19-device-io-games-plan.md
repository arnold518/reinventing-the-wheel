# Device I/O and playable games

Started: 2026-10-19. Status: open (RV32IM and scripted-device roadmap).
Starting code state: branch `realistic-timing` at `e24e289`, with pre-existing and
concurrently changing arbiter, test and visualizer work.

## Summary

The owner selected caches → games → AI accelerators. The primary cache
implementation and its general expansion now exist. The isolated `rv32im-phase3` worktree contains RV32IM, device components,
the scripted system/game runtime and recorded display/keyboard views.
[Phase 6 acceptance](2026-10-21-scripted-games-phase4-6-implementation.md)
remains open; phase 7 interactive sessions have not started. The owner
requested merging the checkpoint into `alpha`; current integration evidence
and remaining checks are in the [wrap record](2026-10-22-rv32im-milestone-wrap-and-integration.md).

The owner has selected **RV32IM** and **predetermined device input before
interactive simulation**. The current sequence is contracts and compatibility
baseline → M arithmetic and core → device components → recorded game runs →
integrated verification/performance → interactive sessions. The
[full roadmap and component inventory](#rv32im-scripted-roadmap)
below supersedes the earlier live-first order and provisional ISA choice.
The owner authorized implementation through phase 6 and phase 7 after acceptance.
The primary ongoing checkout remains untouched while the milestone is integrated.

The owner explicitly requires retaining recorded playback and adding new
live modes alongside it. The completed
[game-playability audit](2026-10-19-game-playability-audit.md) measured
headless update budgets of about 23/7 native and 6.8/2.6 browser updates/s
for compact CPU/FF incremental Pong/Tetris. The current C-ID browser cases
were 0.10/0.039. A slow-clock native profile also verified with pacing
headroom. These support a compact play profile; integrated device/input/frame
performance remains unverified, and no live mode is implemented yet.

Preserve existing RV32I families, pins, encodings, timing, profiles, scenarios
and playback. Add an RV32IM family and reuse existing ALU/storage/memory/
pipeline structures. New devices retain the memory protocol and the
structural-first component path. The planned first batch uses deterministic
input scripts and frame/output assertions; live input will later use those
same device boundaries. Pointer input, glyph/tile rendering, scanout and sound
are later options. Register ABI, concrete names and game acceptance cases are
implementation design details still to freeze.

## 2026-10-19 — Earlier thoughts and current source

Read `docs/STATUS.md`, `docs/README.md`, the record convention, the component
and test guides, relevant public memory/CPU/visualizer pages, the open
future/cache/general-cache records, and the realistic-timing progress log.
Read current interfaces, implementations, test sources and existing dirty
arbiter changes before forming these proposals.

| Area | Observed implementation | Consequence for this milestone |
| --- | --- | --- |
| CPU | `RV32IFiveStageCore.cpp` has separate instruction/data requesters and delayed-response handling; single-cycle and baseline cores require zero-wait FF endpoints. | Use the advanced core as the first integration candidate. Extending the other cores needs separate scope. |
| Protocol | `MemoryProtocol.hpp` defines byte addresses, aligned full-word requests, byte write masks and held responses. `ReadBurst` returns several words per accepted request. | Reuse REQ/RSP, but define device side effects, supported operations and response ownership. A write mask is not a read-size field. |
| Shared memory | `RV32IExperiment.cpp` builds A, B, C-ID, D-0 and D-ID: separate memories, shared uncached RAM, separate L1s, shared L2, or both levels. `TwoPortMemoryArbiter` routes one lower transaction at a time and holds burst ownership. | Shared RAM/arbitration is no longer a missing foundation. The arbiter merges requesters; an address router selects destinations. These are different components. |
| Cache | `Cache.hpp`/`Cache.cpp` have structural control, expanded or scalable banks, burst fills and write-through/no-write-allocate stores. | Device addresses must bypass every cache. Scalable banks do not imply a behavioral whole-cache implementation. |
| Composition | `RV32IProgramRoot` is a testbench under `src/tests`; the experiment runner has another topology assembly. | A reusable platform assembly and runtime ownership need design. Avoid making the fixed testbench the only way to run applications. |
| Guest software | Strict static ELF32 loading, RV32I C builds, startup assembly, linker script and arithmetic helpers already exist. | Reuse toolchain/loader machinery; give games an application startup/linker map and small platform library. The Embench `tohost` convention is not a device framework. |
| Reference | `RV32IInstructionOracle` publicly accepts functional RAM or `MemoryStorage`, not a device bus. | Add an independent device-aware reference boundary. Time/input reads need the specified transaction sampling time; stepping the oracle later at retirement alone does not establish their expected values. |
| Browser/runtime | `VisualizerSession` runs a scenario before collecting timestamps; `stateJson(index)` seeks recorded history. Native `server.py` follows the same pattern. The WASM worker and binding already provide a transport foundation. | Live advance/input/frame APIs are new. Playback seeking is not a complete machine checkpoint from which execution can safely resume. |
| Engine | `Simulator` provides event/deadline-bounded draining and a history-disable switch. | Bounded live execution is feasible without replacing the engine. Long sessions need a defined recording budget and responsive worker scheduling. |

Inspected existing coverage includes `CacheReadTest`,
`TwoPortMemoryArbiterTest`, `RV32IMemoryMigrationTest` and
`RV32ITopologyTest`; this review read their source and did not execute them.
Searches of the module and ISA directories found no existing MMIO/framebuffer
implementation. This is an inventory of those paths, not proof that no
external prototype exists.

The earlier future record's statements that caches/shared arbitration were
absent are dated observations, superseded by this inventory. Current
published cache results live in `site-docs/memory/cache-experiments.md`;
several older summary paragraphs in the cache record and cache component
page predate the campaign. The measured 3–4× improvement is in simulated
cycles for selected workloads, not browser frame rate or host throughput.

## 2026-10-19 — Proposed platform boundary

Start by discussing this composition, with optional cache bypass for
comparison and the existing L1 pair as the first cached candidate:

```text
CPU instruction port → executable-RAM guard → I-cache ─┐
                                                     ├→ arbiter → shared RAM
CPU data port → address router → RAM → D-cache ────────┘
                              ├→ uncached device registers
                              │    console / timer / buttons / display control
                              └→ uncached video-memory endpoint
```

The diagram proposes dedicated display memory. Shared-RAM display scanout
is an alternative requiring another requester or explicit ownership, bus
contention and visibility rules. An L2, if selected, stays on the RAM branch;
device traffic must bypass it as well as the L1s. Align region boundaries to
cache lines and constrain fills to RAM. Instruction requests to device
addresses must fault without touching a peripheral: the core deliberately
permits speculative fetches (`RV32IFiveStageCore.cpp`).

Contracts to settle before construction:

- **Addressing and access.** Versioned, non-overlapping RAM, video and
  register regions; invalid addresses return errors. Define byte masks,
  reserved offsets, reset and X/Z behavior. Existing loads read a whole
  aligned bus word before lane selection, so device reads need consistent
  word semantics; the current pins cannot distinguish LB from LW at the
  same aligned bus address. Do not promise enforcement of LW-only register
  reads without additional information from the requester.
- **Transaction effects.** Choose the edge at which a device samples input
  or changes state. Hold a sampled response under backpressure and apply an
  accepted write once. Reject bursts to register space without touching
  adjacent registers. Define reset during an accepted operation and which
  peripheral state a CPU reset retains.
- **Ordering.** Establish that framebuffer writes are visible before
  PRESENT and that polling reads see fresh device state. The current
  blocking write-through design simplifies this, but it still needs proof
  across the router and peripherals. RISC-V assigns device/memory ordering
  to [FENCE and the execution environment](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html#_memory_ordering_instructions).
  A future write buffer or write-back policy must preserve these rules.
- **Time and input.** Define a timer frequency and its relation to simulated
  time, key down/up state, event ordering and the cycle at which browser
  input becomes visible. Polling is the initial candidate. Record input at
  simulation boundaries for deterministic replay. Host presentation time
  and simulated hardware time must be distinguished; a play mode with a
  different logical time source would need an explicit separate contract.
- **Display.** Choose pixel/grid format, dimensions, stride, palette,
  presentation boundary and tearing/double-buffer behavior. Browser code
  displays exported pixels; guest code owns game state and drawing. A tiny
  structural storage/scanout example must establish the device before a
  scalable display implementation. A framebuffer snapshot API does not by
  itself establish a clocked scanout controller.
- **Live execution.** One portable session contract for load/reset,
  bounded advance, input injection, frame/state observation and pause.
  Pause should stop the defined machine time, not just gate CPU ENABLE while
  peripherals keep running. Replay from reset is an initial option;
  resumable rewind needs real state/event-queue checkpoints.

## 2026-10-19 — Candidate stages and completion evidence

| Stage | Deliverable | Evidence required before calling it complete |
| --- | --- | --- |
| G0: platform contract | Address map, devices, ISA choice, time/input rules, graphics format and a representative rendering workload. | Agreed contracts and explicit performance targets; no guessed frame-rate promise. |
| G1: device path | Structural address router, console, timer and button register; independent reference model. | Held requests/responses, masks, invalid accesses, reset and exactly-once effects; device reads never cached and speculative instruction fetches never touch devices. |
| G2: display | Tiny structural video storage/scanout, then justified scalable storage; test image and moving rectangle from guest code. | Pixel expectations, presentation ordering, structural/compact equivalence and timing evidence for the supported configurations. |
| G3: live runtime | Shared native/WASM session, worker scheduling, input injection and bounded recording. | Scripted input reproduces device/frame traces; pause/reset and long-run memory use behave as specified in both backends. |
| G4: game ports | Freestanding guest library and selected Pong/Tetris ports. | Scripted game-state checks and frame hashes, retained source licenses if porting, then separately measured live controls and playability. |

A first guest console write and a color test should make the early stages
observable. A moving rectangle controlled by buttons exercises the full loop
before a game. Pong first would expose responsiveness and pacing with little
game logic; Tetris emphasizes rotation, row clearing, text and slower timed
updates. Either can be selected first; 2048 remains a useful turn-based
alternative from the earlier shortlist. No new game source was selected or
compiled in this review.

For sizing discussion only, a 160×120 framebuffer at one byte per pixel is
19,200 bytes per buffer, or 38,400 bytes for two. A full redraw is 4,800
aligned 32-bit stores before loop/drawing instructions. Dirty-region drawing
can reduce that traffic. A tile display could reduce CPU traffic further,
but tile/glyph/sprite composition is additional graphics hardware whose
contract and lower-level implementation need their own work.

The [engine-performance record](2026-10-13-engine-performance.md) measured
compact cores at roughly 3,700–13,400 cycles/s on 2026-10-13, before current
cache/timing work. Those figures are historical and cannot establish the
speed of this platform. Measure host cycles/s, instructions/frame,
cycles/frame, frame-transfer cost and input latency for the selected native
and WASM profiles. Completing a correct frame and meeting a useful wall-clock
play rate are separate acceptance gates. Do not tune memory delays to reach
a desired game result.

The pre-game ISA decision remains open as the owner requested earlier.
Current decode/control still implement RV32I only. The standard
[M extension and Zmmul subset](https://docs.riscv.org/reference/isa/v20260120/unpriv/m-st-ext.html)
remain candidates; M includes divide/remainder and Zmmul multiplication.
The earlier pinned-toolchain comparison favored RV32IM, but was not rerun
here. Selecting an extension would add structural arithmetic, oracle,
decode, pipeline/timing and compiled-program work to this milestone.

## 2026-10-19 — Discussion and review boundary

First discussion choices: framebuffer versus a tile-oriented display;
Pong versus Tetris as the main acceptance game; whether ISA expansion belongs
in this implementation stage; and the desired wall-clock responsiveness and
native/browser parity target. Console/timer/button/display register details
can follow the chosen scope. Shared-RAM scanout, interrupts, mouse, sound
and later accelerator DMA each need explicit placement in the plan.

This session changes planning documentation only. Existing source, test,
layout and scenario edits belong to other ongoing work and were preserved.
No build, test, benchmark, browser session or game was run. Public pages
were read, not changed; no site build was needed for this internal record.

## 2026-10-19 — Live interaction and real-time execution

The owner highlighted that replaying a recorded scenario cannot support live
play and asked how genuine interaction and real-time simulation should work.
This investigation refines the runtime proposal and replaces the earlier
placement of live runtime at G3, after the MMIO and display stages. Prove
live execution with a small existing circuit first, then use it for devices
and games. The architecture below remains a proposal for discussion.

### What the engine can already do

`Simulator.cpp` holds mutable current state and a queue of future work.
`processWireUpdateEvent` changes a wire and propagates evaluation; recording
is a separate action. `advanceAndRecord` and `drainUntilIdle` consume queued
events incrementally, and disabling history does not disable simulation.
`ClockGenerator` schedules its next transition rather than constructing an
entire future trace. The existing discrete-event model is therefore usable
for live execution.

The current `VisualizerSession` constructs a test, prepares/runs its finite
scenario, collects timestamps, and seeks back to the first timestamp.
`stateJson(index)` then changes the circuit's displayed pins/wires through
`setCircuitStateAtTime`. That method restores recorded signal values but
does not restore the event queue, clock generator state, compact CPU pipeline
and predictor, component timing state or memory contents. Resuming execution
from a displayed historical time is not supported by that mechanism.

### Proposed ownership and execution

```text
UI controls and input → command queue → live runner → persistent machine
UI screen/inspector  ← published snapshots/frames ←─┘       │
                                                          ├─ circuit/devices
                                                          ├─ simulator queue
                                                          └─ optional recorder
```

- **Simulator:** deterministic virtual time, event ordering and forward
  advancement. No browser or wall-clock reads inside ordinary hardware
  components. Preserve the existing priority of wire updates over component
  evaluations at the same timestamp.
- **Machine session:** owns one circuit, simulator, loaded image, devices
  and input queue for the run. A single execution owner mutates them.
  Expose start, bounded advance, pause/resume, reset, input submission and
  current-state/frame observations. Command acknowledgements and frames
  carry a session generation so messages from before reset cannot affect
  the replacement machine. These are proposed capabilities, not adopted
  class or method names.
- **Live runner:** maps host monotonic time to a desired virtual horizon
  and runs small work quanta. Native execution uses an execution thread;
  the browser reuses the worker transport. Return to the worker's event loop
  between quanta so input/pause messages can be received. A worker running
  one uninterrupted synchronous WASM loop would still starve its message
  processing. The browser UI renders completed output independently.
- **Recorder:** observes execution. Default to history off for play; offer
  selected signals in a bounded buffer and full traces for bounded runs.
  Simulator and memory-storage histories have separate switches today.
  Observation reads current state without seeking the executing machine.
  Historical inspection uses an immutable trace view or a separate replay
  instance. Checkpoint/restore is later work with an explicit full-state
  contract.

For example, a key-down command arrives between execution quanta. The owner
assigns its effective tick and sequence number, schedules the device input
change, and records that effective placement. The button hardware captures
it at its specified edge with its required setup time. CPU software reads the
device, changes game state and writes display storage. A completed-frame
notification produces an immutable pixel snapshot for the browser. Key-up
follows the same path; preserve transition ordering. If brief taps must
survive long guest polling intervals, the device needs an event FIFO or
latched-edge contract in addition to current key state.

The live-mode input policy can place commands at the next safe future
boundary. A mode aligning external events to wall time instead needs a
defined host-to-virtual mapping, a late-input policy and an overload policy.
Do not backdate an input after its virtual time has executed. Record applied
ticks, not just host arrival times, for deterministic replay. Bound the
command queue and report overload rather than silently losing transitions.

### Kernel changes identified from source

1. **Use a dedicated 64-bit simulation-time type.** `Event::time`, simulator
   time/deadlines, component publication times and memory histories currently
   use `size_t`. The inspected `build-wasm` compiler configuration reports
   `CMAKE_CXX_SIZEOF_DATA_PTR=4`; the build has no wasm64 override. On this
   wasm32 target, the 32-bit tick range at one picosecond per tick is
   `2^32 ps = 4.294967296 ms`. This is a representational limit inferred
   from source/configuration, not a newly reproduced overflow. Migrate all
   absolute-time paths and checked additions, including gate-twin and
   publication bookkeeping, rather than only the public simulator field.
   Keep sizes/indexes distinct. Exchange large exact timestamps with JS
   through BigInt or decimal strings; raw JSON numbers eventually lose
   integer precision. This requires no change to the target's pointer size.
2. **Protect monotonic execution.** `scheduleEvent` currently has no
   past-event check. Add an execution contract that rejects an event before
   the committed frontier, while allowing internal same-timestamp work in
   an open timestamp. An external command must not reopen a completed
   timestamp. Track the processed-through horizon separately from the last
   event time: today advancement can finish before its requested deadline
   when there is no event at that deadline.
3. **Distinguish yielding from a publishable boundary.** The current event
   count limit can stop midway through all the work at one timestamp. A live
   runtime may yield there privately to service its message queue, but
   should enqueue commands and keep the last completed snapshot until a
   coherent boundary is reached. Track whether a timestamp is open. Use
   both an event budget and a wall-time budget, and a watchdog for excessive
   work at one timestamp. Hardware input capture must also honor setup and
   hold; choosing a timestamp alone does not prove a safe clock edge.
4. **Give pause/reset/inspection their own semantics.** Pause stops virtual
   machine advancement; merely deasserting CPU ENABLE leaves clocks and
   peripherals running. Resume reanchors pacing to exclude paused host
   time. Reset needs a defined whole-machine lifecycle; rebuilding is the
   simplest initial way to discard all old queue/component state while
   reloading the image. Inspectors must not call historical seek on a live
   instance. Choose fidelity at construction; switching a running part to
   another implementation needs state migration or a reset/replay.

### Pacing and performance

Two clocks exist: virtual hardware time and host elapsed time. In common
time units, a paced runner targets:

```text
desired_virtual_time = virtual_anchor + speed * (host_now - host_anchor)
```

For realistic profiles, tick-to-second conversion comes from the timing
profile. Abstract `unit` ticks require an explicitly configured rate. Run
toward the desired horizon within the work budget. If ahead, yield/wait; if
behind, report the lag and follow a selected policy. A game runtime could
slow virtual progression or pause on excessive lag. Changing the time anchor
or speed must be explicit. Do not skip CPU cycles, memory refresh or other
hardware events to conceal a missed deadline. Discarding an old *host frame
copy* is different: guest computation and device effects still execute.

| Mode | Contract |
| --- | --- |
| Step/debug | Advance specified ticks, events or clock edges; no wall-time target. |
| Live, unrestricted | Run at available throughput and accept input at future virtual boundaries; measure actual speed. |
| Live, paced | Try to maintain the selected virtual/host speed, with bounded work quanta, reported lag and a specified response to overload. |

Rendering at 60 host frames/s does not imply that the guest has computed
60 frames/s. At the current modeled `embedded` period of 4,104 ps, matching
one simulated second per host second requires about 244 million CPU cycles
per host second. That is a rate calculation, not a measured capability.
Historical thousands-of-cycles/s measurements do not establish readiness at
that rate. A selected slower modeled clock can preserve delay contracts but
changes CPU throughput relative to timer/display hardware and needs its own
platform definition; simply lowering that rate does not prove enough guest
instructions per game frame either.

Begin with proven compact component choices, history off, selective state
observation and efficient binary frame export. Benchmark the actual system
before committing to a play-rate target. If it remains too slow, event-path
optimization, a compiled circuit evaluator, or additional validated compact
components are separate engineering options. A faster application-oriented
execution model would need a declared fidelity/validation boundary; do not
silently remove gate/memory timing to obtain a game result. The existing
[engine-performance plan](2026-10-13-engine-performance.md) is relevant input,
not current throughput evidence.

### Revised proposed order and review boundary

1. Define live-session, time, input and pause/reset contracts; harden time
   representation and advancement boundaries.
2. Prove interaction with an existing clock and flip-flop: change D while
   running, observe subsequent Q, pause, resume and reset in native/WASM.
3. Add pacing, bounded observation and deterministic input replay. Verify
   replay independence from execution-quantum size, long time ranges,
   bounded memory and input latency under load.
4. Integrate MMIO/device/display stages from the earlier proposal; measure a
   representative guest rendering loop before game ports and rate promises.

External references read for this design:
[Emscripten's cooperative execution model](https://emscripten.org/docs/porting/emscripten-runtime-environment.html#browser-main-loop),
[its wasm32/wasm64 and exact i64 JS interface settings](https://emscripten.org/docs/tools_reference/settings_reference.html),
and [ns-3's virtual/wall-clock pacing and lag policies](https://www.nsnam.org/docs/manual/html/realtime.html).
These support the runtime distinctions; their implementations are not
components of CircuitSim.

This follow-up read source and build metadata and updated planning documents.
No simulation, overflow reproduction, tests, performance run or live browser
check was executed. Source and existing concurrent edits were preserved.

## 2026-10-19 — Additive modes and measured playability required

The owner clarified that recorded playback must remain supported: new live
and paced modes are additions. This replaces any interpretation of the
runtime proposal as a replacement for the current viewer mode. The owner
also requested thorough empirical verification of game FPS, including low
rates. The [playability audit](2026-10-19-game-playability-audit.md) owns the
workloads, commands, results and uncertainty for that investigation.

## 2026-10-19 — Playability audit completed

The [closed audit](2026-10-19-game-playability-audit.md) records 57 completed
native/browser runs, 23 instruction-oracle checks, zero setup violations,
cross-configuration final-state agreement and per-update sanitized host
renderer comparisons. Incremental drawing is viable within a small
compact/FF budget; full redraws, the structural CPU and the present
gate-controlled cache hierarchy are much slower. This updates the earlier
absence of throughput evidence; it is not an adopted first-game choice or
a delivered live-play result.

The next runtime proof should retain recorded playback and measure actual
input-to-output latency, device overhead and frame pacing. A native 4 kHz
CPU computed the prototype workloads faster than their modeled time; the
corresponding browser preset still requires the time-width migration.
Do not infer a DRAM refresh/retention contract from the FF slow-clock result.

## 2026-10-19 — Component contracts, graphics, ISA and a broader game set

The owner requested components that fit the existing system, a renewed ISA
assessment, and more varied game candidates. This section makes the proposal
concrete for discussion; names, addresses and choices below are **drafts**,
not implemented interfaces or adopted ABI. Code inspected: `739c454`.

Re-read the component guide, fidelity reference, public transaction/memory/
advanced-core/system-instruction pages, and the current protocol, controller,
arbiter, cache, storage and decoder sources. The existing structural parts
establish the construction path; no arbitrary C++ device callback is proposed
as the original hardware implementation.

### Hardware decomposition and reuse

Use a reusable machine assembly outside `src/tests`. Its hardware/topology
configuration selects RAM technology, cache arrangement and geometry; its
single `BuildProfile` selects component fidelity. Execution mode is a separate
runtime choice: recorded playback, stepping, unrestricted live or paced live.
The same platform contract should work with different fidelity selections.

| Proposed part | Existing parts to reuse and structural starting point | External contract and stage |
| --- | --- | --- |
| `AddressRouter32` | Address comparators/decoders, muxes, `Register<32>`, `ClockedStateBit`, response buffers. Start with two destinations and an invalid-address target. | One incoming REQ/RSP port, one port per destination, CLK/RST. Capture destination ownership on acceptance; translate addresses to endpoint-local byte offsets. First stage. |
| `InstructionAccessGuard32` | The same address/permission check and error-response machinery. | Guard IMEM before I-cache; only executable RAM ranges reach the memory path. Device fetch requests receive a side-effect-free error; only a surviving instruction may raise an architectural fault. First stage. |
| Device transaction/register frontends | Reuse `MemoryTransactionPort` and `MemoryResponseBuffer` at width 32, plus explicit request capture, address validation, masked register writes and action strobes. These port classes take width as a constructor argument. | Identical memory-protocol pins, rising-edge acceptance and held responses. Factor shared structural glue without hiding each peripheral's state machine. First stage. |
| `ConsoleDevice32` | A byte register and small structural FIFO, full/empty state and write strobes. | TX data/status registers and observable output bytes. Bounded capacity and defined backpressure. This is a memory-mapped console; serial baud/line emulation would be additional hardware. First stage. |
| `TimerDevice32` | Incrementer, divider/enable counter, `Register<32>`, comparator and status latch. Begin with a small-width counter that wraps in tests. | Readable 32-bit tick count and fixed rate description; optional polled compare/deadline status. Driven by the machine clock, never by direct host wall-clock reads. First stage. |
| `ButtonDevice32` | Input sampler, 32-bit level register, transition encoding and small FIFO built from register words, read/write pointers and occupancy state. | Level mask, FIFO head/status, explicit POP write and sticky overflow flag. Host transitions enter through specified input pins at safe virtual boundaries. First stage. |
| Video RAM | Existing `memory.ff.endpoint.32` or scalable FF endpoint with a visible controller and selected native storage. | Uncached byte-masked stores and full-word reads, using the ordinary bus contract. A tiny structural screen first, then a 64×48 screen with scalable storage. First stage. |
| `FramebufferDisplay32` | Structural geometry/status/presentation registers and the video-memory child; a tiny pixel-address/byte-selector/palette example establishes pixel interpretation. | Read-only geometry/format, PRESENT command and frame sequence, plus a read-only frame capability for host export. The first display is the snapshot contract below. First stage. |
| `PointerDevice32` | Coordinate/button registers and the input/FIFO building blocks above. | Logical-pixel coordinates and click transitions; keyboard cursor remains an option for initial puzzle games. Second stage. |
| `GlyphTileDisplay`, optional fill/blit unit | Glyph ROM, pixel selector, tile/attribute RAM; later address/count sequencer and memory requester for fill/copy. Prove a glyph or small tile and a short copy first. | Hardware graphics operations through declared registers/memory. Optional later device contracts, with their execution cost and memory ownership model visible. |
| Optional sound output | Divider, duration counter and one-bit square-wave output; a sample FIFO/DMA is a larger follow-up. | Tone frequency/duration and enable registers. Host audio follows virtual transitions. Games first work with sound disabled. |

Place device modules under a proposed `include/modules/device/` and
`src/modules/device/`; shared routing may belong under a bus module. Draft
catalog namespaces are `bus.*`, `device.*` and `video.*`; confirm them against
the catalog before registration, since saved profiles/layouts persist IDs.
Keep addressed storage under the existing `memory.*` families and register
state under `storage.*`. A display must not introduce a second memory-storage
engine.

For each new public family: one structural implementation, at most one
behavioral implementation with identical pins/semantics, parameter validation,
manifest entries, structural or representative-slice evidence, independent
expectations, fidelity equivalence and proven timing. A parameterized tiny
screen establishes representative behavior; it does not prove a full-size
screen's timing by itself. Extrapolated timing needs an explicit derivation
and validation under the timing-table policy. Keep small control circuits
structural when their measured event cost is acceptable.

### Draft address map and transaction rules

| Byte range | Proposed mapping |
| --- | --- |
| `0x00000000–0x0003ffff` | Existing 256 KiB RAM envelope; reset entry remains zero. Executable permissions may later refine the RAM guard. |
| `0x10000000–0x100000ff` | Console registers |
| `0x10001000–0x100010ff` | Timer registers |
| `0x10002000–0x100020ff` | Button registers |
| `0x10003000–0x100030ff` | Display registers |
| `0x10004000–0x100040ff` | Reserved pointer window; returns Range until a device exists |
| `0x20000000–0x20000fff` | Initial 4 KiB video endpoint; 3,072 bytes are visible pixels, remaining bytes are ordinary unshown padding |

Unused register offsets and unmapped addresses fault. These aligned regions
keep RAM/cache fills away from devices. RAM capacity is a platform parameter,
not an ISA limit; ELF loading, linker/stack layout and the address guard must
agree with it. Adding larger displays later changes explicit geometry/storage
configuration, rather than silently overflowing this first window.

The router is above D-cache, not below it. Console/timer/input/display/VRAM
traffic bypasses every cache; optional L2 remains on the RAM branch. Existing
`TwoPortMemoryArbiter` merges I/D RAM traffic after selection and cannot
substitute for a destination router. There is no display DMA requester in
the initial snapshot design.

- Use the exact 32-bit REQ/RSP schema and status values. Device read data is
  captured at its defined transaction sampling edge and retained unchanged
  under response backpressure; timer/input values must not keep changing
  inside a held response.
- Side effects occur once on a valid accepted action, independent of
  response retries or stalls. A zero write mask performs no action. Pixel
  stores accept the existing byte lanes. Command registers initially require
  a full word mask for a nonzero-mask write; partial commands return
  InvalidRequest without changing state. Define validation precedence,
  reserved bits and X/Z behavior in each contract before implementation.
- Reads are aligned whole bus words, even for guest LB/LH: the current
  protocol carries no read-size field. Keep input-head/status reads
  nondestructive and use explicit POP/ACK writes. A compiler using a byte
  load must not accidentally consume an event.
- Keep a selected route through completion and hold responses when the
  requester stalls. State-based request readiness must preserve the existing
  rule against a same-cycle RSP_READY-to-REQ_READY path. General FF response
  retention is the safe initial choice; `always_ready` is allowed only after
  the actual routed consumer proves its contract.
- Reset has two levels: bus RST clears interface ownership/held responses;
  a whole-machine reset reconstructs or explicitly initializes peripheral
  state and reloads memory. Existing memory RST does not clear stored bytes.
  Do not silently reinterpret it as a whole-device content reset.
- Guest library accessors use volatile accesses, a compiler ordering barrier
  and the platform's verified FENCE ordering around presentation. Audit the
  core's current FENCE-as-no-op contract against all selected paths before
  relying on store-to-PRESENT visibility. Future write buffers/writeback
  caches need a fresh ordering/coherence design.

**Burst integration finding.** The external protocol has no response-LAST
pin. Current `MemoryArbiterState` counts `block_words` responses for a granted
ReadBurst and does not inspect RSP_STATUS to terminate it early
(`TwoPortMemoryArbiter.cpp:97–113`). Therefore the earlier broad suggestion
to return one error for a device burst must be scoped: the CPU-side device
router rejects it before any grant to the shared RAM arbiter; normal CPU
loads/stores issue single-word operations. A standalone register device can
reject unsupported bursts with one error under that caller contract. A
future general router/DMA path that forwards bursts must first define and
test the system-wide error-termination rule. Do not insert a one-response
error target after this arbiter and assume it releases ownership.

### First display: persistent drawing memory and explicit frame snapshots

Propose **64×48, one 8-bit palette index per pixel, stride 64, fixed palette**.
That is 3,072 bytes and four pixels per 32-bit word; a pixel address is
`VRAM_BASE + (y << 6) + x`. Software can update a few pixels/cells and write
PRESENT once. Host scaling makes the logical screen readable; scaling
creates no additional guest game updates. Larger configurable resolutions
and a writable palette come after measuring this system.

Keep the drawing surface persistent across presentations. On PRESENT the
runtime captures an immutable pixel/palette snapshot identified by machine
epoch, virtual capture time and frame sequence. A proposed `FrameStateView`
capability exposes this observation without depending on the concrete memory
or display class. The native/browser host converts pixels to colors and
draws them; the guest performs game logic, collisions and initial pixel
drawing. Console and input bridges likewise use implementation-independent
capabilities/pins, not SDL, browser or Python calls inside circuit modules.

The capture boundary is part of the contract: complete all relevant
same-timestamp events, capture after earlier VRAM transactions are visible,
and before any later guest write can modify the image. The live runner needs
a stop/observation boundary for this; noticing a sequence change after an
arbitrary multi-cycle batch and then reading RAM is insufficient. An
initial runner may advance to clock boundaries and inspect the presentation
state before proceeding; its overhead must be measured. Export a copy, not
a live view of guest memory. Intermediate host frame copies may be dropped
under display backpressure; guest actions and presentation sequence remain.

This models a framebuffer device with a presentation strobe and an external
screen observer. It does **not** claim to model VGA/HDMI serial timing,
continuous raster scanout, VRAM read contention or hardware copying. The
tiny structural pixel selector and register/bus scenarios establish the
device behavior; a host snapshot by itself is not a scanout circuit.
Unknown pixel bits need the same explicit debug visualization/mask in both
hosts, rather than silently treating X as an ordinary color.

This refines the earlier unresolved buffer proposal. A simple alternating
front/back pair does not preserve incremental drawing: after a swap the next
drawing buffer contains an older image. An eventual double-buffer contract
must require updating both copies, copying the front image, or managing
buffer age/dirty regions. Those costs would invalidate a direct reuse of the
single-surface benchmark budget. The snapshot contract avoids this issue.

A later **raster display** is a distinct explicit feature: pixel/line/frame
counters, pixel address generator, byte/palette selector, sync/blanking,
scanout requester, frame boundary and buffer ownership. Shared-memory scanout
also needs arbitration and visibility rules; the existing two-port arbiter
has no free third requester. A tiny raster display must precede a scalable
form. Measure its device events; a fast host canvas is not proof of cheap
simulated scanout.

Glyph/tile display is a useful subsequent hardware feature for board games,
mazes and text: the CPU writes tile IDs/attributes and the display derives
pixels using a glyph ROM. This reduces guest writes through new simulated
hardware, so give it its own structural pixel/tile proof and public contract.
It does not require a new CPU instruction set.

### Timer, input and guest software

Start with polled devices. Timer ticks derive from the configured CPU clock
and divider, with frequency exposed in the platform description. For example,
4 kHz / 40 or 1.5 kHz / 15 gives 100 Hz; these are proposed presets, not
validated full-system clocks. Use wrap-aware 32-bit deadline arithmetic with
bounded intervals. A 32-bit guest timer is independent of the required
64-bit simulator time: engine time width does not require RV64 or a
`Pin<64>`. A future wide timer can use two 32-bit registers and a specified
coherent snapshot rule.

Buttons expose both current levels and queued transitions, so a tap between
guest polls remains observable. Use logical button IDs for two players,
direction/action/start/pause, not host-specific key codes. The host applies
transitions at future safe input boundaries with sequence ordering; ignore
redundant browser repeat keydowns. A small FIFO has an explicit full policy:
proposed drop-new with sticky overflow while still updating the level mask.
Record overflow/loss in replay evidence. Specify concurrent push/POP and
overflow/clear priority; do not hide those cases in a host queue. Pausing
freezes the entire virtual machine. Define queued releases/resume handling
to avoid stuck held buttons.

Add a small freestanding guest library with `buttons`, `event_peek/pop`,
`ticks`, `console_write`, `pixel/span/rect`, `present` and a deterministic
seeded PRNG. Supply startup/linker/stack definitions and tiny software glyphs
for scores. Games compile to the same ELF for native/WASM; frontend code
does not implement game rules. A reference address-space/device model remains
independent of the circuit and samples the same specified timer/input values
at transactions, rather than fabricating them at later retirement checks.

### ISA assessment and the explicit pre-game decision

**RV32I can express everything required by this device platform and the
initial games.** Loads/stores access pixels and device registers; arithmetic,
branches and bit operations implement game rules. The completed audit
actually executed Pong/Tetris logic and drawing as RV32I, so that narrow
functional claim now has simulator evidence. General multiplication/division
can use software helpers; fixed-point arithmetic and lookup tables can avoid
floating-point hardware. This does not establish performance of an arbitrary
upstream game port.

The base specification explicitly leaves device-address interpretation to
the execution environment and defines ordering for I/O with FENCE:
[RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html).
The proposed MMIO ABI and hardware are the missing platform. Polling is
sufficient for the first release. Interrupts, resumable traps, CSRs and
privilege support are a separate CPU/platform extension; current terminal
ECALL cannot provide OS syscalls or interrupt handlers.

| ISA option | Assessment for this milestone |
| --- | --- |
| RV32I | Proven functional baseline for the measured prototypes. Lowest CPU change scope; preserves existing compiled-program evidence. |
| RV32I + Zmmul | Standard multiply-only option: four RV32 multiply instructions. Smaller arithmetic scope, but the pinned compiler/runtime problem in the earlier record remains unresolved. |
| RV32IM | Provisional expanded target if choosing arithmetic before games: all eight RV32 multiply/divide/remainder operations, supported by the previously checked compiler/multilib. More hardware/control work; host-speed improvement is not established. |
| RV64, C, F/D, A, vector or a custom game ISA | No initial device/game requirement establishes a need. Reassess for a concrete workload rather than incorporating them into the first platform. |

M and Zmmul are standard extensions, not a new unrelated ISA:
[ratified M/Zmmul specification](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html).
M denotes multiply/divide; machine privilege is a separate concept. RV32IM
alone does not add interrupts or an OS.

The previous recommendation to evaluate RV32IM before the first game remains
a proposal, and the owner has not selected an extension. Preserve that
decision gate instead of quietly deferring it. Current source still rejects
R-type funct7 `0x01` and has no multiply/divide ALU contract. The recorded
GCC 12.4 check favored RV32IM because it emitted MUL/DIV and selected its
multilib; Zmmul instead used helpers and an incompatible compressed-code
runtime. This follow-up did not rerun that compiler check.

If adopting M: first build small structural multiplier/divider slices, then
a 32-bit arithmetic unit with explicit START/BUSY/DONE and result halves as
two 32-bit ports. An iterative design can reuse add/shift/register parts and
avoid an unproven giant single-cycle path. Integrate execution waits, hazard
handling, flush/cancellation and one retirement; retain independent oracle
semantics including high products, signed division overflow and divide by
zero. Add catalog/timing/equivalence/compiled tests and an explicit ISA
identity/configuration preserving RV32I contracts. Measure host elapsed time
and event count as well as saved guest instructions. A many-cycle structural
multiplier can lose host throughput despite reducing instruction count.

The device ABI can be identical for RV32I and RV32IM. My proposal is to
retain RV32I compatibility, use it to prove the platform, and make RV32IM
the candidate expanded build if the pre-game arithmetic gate is adopted.
Neither choice requires redesigning device I/O or graphics.

### Diverse game candidates and what they would exercise

These are prospective small versions/original implementations or source-port
candidates, not compiled or measured game deliveries. Rates below describe
design suitability, not a new FPS forecast. Only the audit's minimal
Pong/Tetris prototypes have throughput evidence.

| Candidate | Different behavior exercised | Suitability and next dependency |
| --- | --- | --- |
| Sokoban | Spatial puzzle, tile map, move/undo, small levels | Strong first candidate: redraw on moves, no fixed-rate simulation. Keyboard only; use original or explicitly licensed levels. |
| 2048 | Merge rules, bounded board and deterministic random spawning, number glyphs | Strong first candidate, event-driven and compact. The MIT C reference needs terminal/time/random adaptation. |
| Minesweeper | Pointer/cursor input, flood reveal, flags and digits | Keyboard cursor first, pointer later. Large reveal is a useful burst-work/latency test. |
| Reversi or Connect Four | Turn-based strategy, two-player controls, optional bounded search | Start local two-player; bounded-depth AI later is a CPU workload with separately measured think time. |
| Tiny turn-based dungeon | Exploration, glyphs/tiles, inventory and deterministic procedural maps | Broader gameplay without continuous animation; useful after the small text/tile library. Keep map/entity count bounded. |
| Snake | Grid motion, short input taps and fixed virtual deadlines | Early real-time candidate: dirty cells and selectable slow speed. Queue direction changes instead of losing them between polls. |
| Pong | Two simultaneous players, simple motion/collision and held buttons | Existing prototype anchor; use large paddles/slow ball at low rates. Measure input latency, not just frames. |
| Tetris | Rotation/edge input, repeat timing, gravity and occasional large board changes | Existing prototype anchor, but line clears already show expensive bursts. A slow/practice mode is a more credible initial target. |
| Breakout | Paddle plus many brick collisions, local destruction and score | Incremental arcade follow-up; cheap in calm frames, multi-hit cases need profiling. |
| Space Invaders style game | Projectile pools, multiple moving objects and formation updates | More entity/dirty-region pressure than Pong; stage after simpler arcade games. |
| Asteroids style game | Rotation, fixed-point physics, wraparound and many collisions | Later arithmetic/rendering stress; tiny integer/lookup-table design or substantial adaptation of the float/trigonometric reference. |
| Small platformer | Scrolling/viewport, jumping and collision with a tile map | Later tile-display candidate; continuous motion and larger changed area make it a poor initial low-FPS acceptance game. |

A balanced first set would be **Sokoban or 2048 + Pong**, then **Snake and
Tetris**, then one strategy/exploration game and one projectile/platform game.
This tests more of the platform than several similar arcade demos. Initial
arcade speeds must be tuned for the chosen virtual CPU budget; a game
designed for 60 updates/s cannot retain that assumption at 1–3 updates/s.

Current upstream source references:

- [2048.c](https://github.com/mevdschee/2048.c) advertises MIT licensing;
  [its C file](https://github.com/mevdschee/2048.c/blob/main/2048.c) uses
  terminal APIs, host time and random initialization that must be replaced.
- [raylib-games classics](https://github.com/raysan5/raylib-games/tree/master/classics)
  contains Snake, Tetris, Arkanoid, Space Invaders, Asteroids and a platformer.
  The repository states zlib licensing for game sources. These are source
  references, not a proposal to port raylib/OpenGL. Extract/adapt game logic
  and replace platform/drawing calls with the guest library; pin a revision
  and review each selected file and asset when choosing a port.
- [Asteroids source](https://github.com/raysan5/raylib-games/blob/master/classics/src/asteroids.c)
  directly uses floating-point vectors, trigonometry and a 60-FPS loop;
  it is not a drop-in RV32I integer game.

DOOM remains a later capacity/throughput experiment, not the first live-play
acceptance target. CHIP-8 emulation could later offer a varied 64×32 software
VM workload; it would execute on RV32I and would not require replacing the
hardware ISA. Interpreter overhead, timers, input semantics and ROM
availability would need a separate feasibility/port audit. Life/Mandelbrot
can be display/compute diagnostics, but are not substitutes for playable
games.

### Proposed stage gates and verification scope

1. **Preserve playback; prove the live runner.** Complete the earlier
   64-bit-time, bounded-quantum, safe observation/input, pause/reset and replay
   proof using existing clock/flip-flop parts in native/WASM.
2. **Settle the contracts and pre-game ISA choice.** Review the draft bus map,
   register masks/side effects, snapshot display model and initial presets.
   If M is adopted, complete its lower-level/oracle/timing path before
   compiling M games; the device API remains unchanged.
3. **Build structural bus/device slices.** Two-target router, fetch guard,
   counter, FIFO, console and tiny pixel memory/presentation. Test invalid
   addresses/ops, X/Z, stalls, response stability, reset, partial/zero masks,
   route ownership, FIFO races and attempted speculative device fetches.
4. **Assemble the actual platform.** Add scalable VRAM and the guest library;
   execute an interactive cursor/pixel demo and then a turn-based game.
   Verify instruction/device reference results, deterministic input replay
   and frame hashes across native/WASM and execution quantum sizes.
5. **Measure integrated play.** Re-run equivalent dirty-drawing game loops
   through the chosen RAM topology, router, devices and frame capture.
   Measure input-to-pixel median/p95/worst latency, completed guest updates
   and visible frames separately, queue occupancy, memory growth, events,
   setup violations and pacing lag. Include line clears/flood reveals and
   long paused/resumed sessions. Only then set a delivered rate target.
6. **Expand graphics/games.** Add pointer/tile/sound/scanout or arithmetic
   acceleration against a selected game need, retaining a visible
   structural example and the same component registration/evidence process.

The closed audit's fast baseline used separate compact FF endpoints. A new
shared-RAM arbiter, device frontends or a different VRAM endpoint can reduce
that rate before the host renderer is involved. The cached/DRAM results
already show a much larger cost. Neither the approximately 3 Pong / 1 Tetris
browser planning budget nor the 4 kHz native pacing result is an integrated
platform guarantee. Keep a measured compact play configuration and the
slower detailed/cache configurations available under the same device ABI.

This follow-up changed planning/status/changelog only. It inspected source
and primary ISA/upstream game references; it did not build new components,
run tests, remeasure FPS, rerun the earlier compiler probe or check the
published site. The Simon Tatham site lookup timed out and was not used as
a source or an assessed port candidate.

<a id="rv32im-scripted-roadmap"></a>

## 2026-10-19 — Adopted direction: RV32IM and predetermined device runs

### Owner decisions and superseded proposals

The owner selected RV32IM, instructed that components/ISA/games come before
interactive simulation, and required preservation of the existing components
and structures. These decisions supersede the live-first sequences above,
the earlier proposal to prove the device platform with RV32I first, and the
open RV32IM/Zmmul choice. Recorded playback remains supported; new live modes
remain later additions. The implementation breakdown below is the proposed
roadmap under those decisions.

Planning source state: branch `realistic-timing`, HEAD `684481a`. Existing
uncommitted work includes RV32I scenarios, registry/CMake entries, bindings,
memory inspection and viewer code as well as the earlier planning/audit
files. This follow-up changes planning documents only and preserves those
source edits. The closed playability audit remains frozen evidence.

### Preservation requirements

1. **Separate ISA identity.** Add `RV32IMFiveStageCore`, with proposed family
   `rv32im.core.educational-five-stage`, and an RV32IM device-system root.
   Keep every existing RV32I family, TypeName, constructor default, pin schema
   and accepted-instruction set. Old RV32I decode/cores continue to reject M.
   The single-cycle and baseline RV32I cores remain available as they are.
2. **Reuse the hierarchy.** Keep ALU8/ALU32, add/subtract/shift/compare units,
   register file, storage, packet registers, forwarding, prediction, memory
   requesters, endpoints, caches and arbiter as reusable building blocks.
   Do not add MUL/DIV opcodes to the existing ALU contracts or replace the
   memory hierarchy to enable games. New M execution sits beside the ALU.
3. **Add M control explicitly.** Introduce M decode/decode-stage/execute/
   coordination contracts, including execution waits. Do not pack M tags
   into reserved old control bits or reinterpret existing operation numbers.
   Existing packets already retain the instruction, which may permit M
   classification without changing their schema. If another field is truly
   needed, use a new M packet contract. Freeze that choice in phase 0.
4. **Share implementation carefully.** Reuse families and extract only narrow
   private assembly/rule helpers where needed to avoid large copy-pasted
   cores. A shared-helper extraction is reviewed and checked independently
   before adding M behavior. Existing instance paths/layout identities must
   remain stable. Avoid both an unrelated second CPU architecture and a
   sprawling extension switch in every existing part.
5. **Preserve protocol and timing.** Keep memory operation/status values,
   aligned reads, lane masks, reset meaning and VALID/READY rules. Add
   routers/devices outside the caches. Keep old proven delay tables and
   certified clocks; derive separate M/device timing and operating periods.
6. **Preserve selection and frontend behavior.** New families enter the same
   catalog, one profile and manifest system. Existing profile names/rules,
   IDs, scenario aliases, CLI defaults, ELF fixtures, saved layouts and
   playback behavior remain valid. New CLI/build options are explicit and
   scoped; never change global compiler flags to RV32IM.
7. **Protect this worktree.** Inventory unrelated edits before each patch.
   Use isolated serial builds with Python deployment disabled. Keep each
   arithmetic, core, device, runtime and documentation change reviewable.
   Rebaseline existing outputs only for an independently established existing
   bug, never simply to accommodate an extension regression.

The goal is compatibility backed by evidence. It does not imply that no
existing file can be edited: catalog/build registrations are necessary, and
small shared-helper or time-type changes need especially strict regression
gates. Observable old behavior is the boundary to protect.

### New hardware component inventory

Names are concrete planning names, not yet registered ABI. Width variants
and structural/behavioral implementations are variants of one logical
contract, not extra components. Plan these 22 logical components:

| # | New component | Purpose / reuse |
| ---: | --- | --- |
| 1 | `MultiplyStep<W>` | Structural partial-product/add/shift step; a small W=4 instance establishes the arithmetic path before scaling. Reuse existing adders/gates. |
| 2 | `DivideStep<W>` | Structural restoring-division compare/subtract/select step; a small instance establishes remainder/quotient recurrence. Split carry/extra remainder bits across supported pin widths. |
| 3 | `Multiplier32` | Iterative 32×32 multiplication; two 32-bit result halves, signed/high-product handling, explicit start/busy/completion. |
| 4 | `Divider32` | Iterative signed/unsigned quotient and remainder, sign correction and ISA-defined exceptional operand results. |
| 5 | `MulDivUnit32` | Own one accepted arithmetic operation, select multiply/divide and the required result, retain completion until consumed, cancel/reset safely. |
| 6 | `RV32IMDecodeControlUnit` | Recognize all eight M instructions and combine with existing I decode through an explicit new contract. |
| 7 | `RV32IMDecodeStage` | Register reads, existing bypass/hazard blocks and M-aware decode; preserve the original I decode stage. |
| 8 | `RV32IMExecuteStage` | Existing ALU/branch/operand forwarding plus the new arithmetic unit and result ownership. |
| 9 | `RV32IMPipelineCoordinator` | EX wait/acceptance/completion, older-work drain, cancellation and one-time retirement, combined with memory waits/flushes. |
| 10 | `RV32IMFiveStageCore` | New advanced core family composed from reused I blocks and new M blocks, with the same external memory protocol. |
| 11 | `AddressRouter32` | Registered destination ownership, permission/range decode, local address translation and response routing. |
| 12 | `BusErrorTarget32` | Held, side-effect-free protocol error responses for invalid or forbidden requests. Reject bursts before an incompatible shared-bus grant. |
| 13 | `InstructionAccessGuard32` | Restrict fetch to allowed memory before I-cache; speculative faults have no device side effects and can be squashed. |
| 14 | `DeviceRegisterPort32` | Common structural device frontend using existing transaction/response-buffer parts, validation, request capture and one-time action strobes. |
| 15 | `ClockedCounter<W>` | Counter/enable/divider building block for virtual timer ticks and small counter scenarios. |
| 16 | `SynchronousFifo<W>` | Small register-based queue with explicit push/pop/full/reset rules; reused for input and console. |
| 17 | `ConsoleDevice32` | Memory-mapped byte output/status and bounded output buffering. |
| 18 | `TimerDevice32` | Readable deterministic ticks/rate and polled deadline status, using the same machine clock. |
| 19 | `ButtonDevice32` | Current logical-button levels plus transition FIFO and sticky overflow; scripted pins first, live pins later. |
| 20 | `IndexedPixelSelector32` | Select one of four 8-bit pixels from a word; tiny palette lookup uses existing ROM/logic parts and establishes the observed color mapping. |
| 21 | `FramebufferDisplay32` | Persistent drawing-memory composition, geometry/format and PRESENT/sequence registers, with read-only frame observation. |
| 22 | `RV32IMDeviceSystem` | Reusable CPU/RAM/cache/router/peripheral assembly outside test fixtures, constructed with one profile and manifest. |

Video RAM itself reuses the existing FF/SRAM device/endpoint families; no new
storage implementation or new fidelity category is planned. Representative
arithmetic steps may be internal children rather than independent public
catalog entries if their use does not justify a public contract. Apply the
same source/scenario/equivalence/timing discipline either way.

Use `include/modules/rv32im` and `src/modules/rv32im` for the new ISA-specific
blocks; arithmetic belongs beside current composite arithmetic, and device/
routing modules get their own appropriate owners. Preserve old names such
as `RV32IMemoryRequest`: that name means RV32I + MemoryRequest, not the new
RV32IM extension, and does not need a rename.

### Supporting software and test equipment

These are not additional simulated hardware components:

- **RV32IM decoder/reference oracle:** independently specified M operations,
  a tagged I/M instruction representation and compatible architectural-state
  observation. Retain existing enum values and the RV32I oracle's public
  behavior. Reuse independent I semantics through a narrow extraction/adapter
  if necessary; never call a product behavioral arithmetic rule as its oracle.
- **Reference device address space:** independent register/FIFO/timer/video
  semantics and accepted-transaction observations for instruction/device
  comparison. Timer samples occur at the defined hardware read edge, not at
  a later oracle step chosen for convenience.
- **Scripted stimulus driver:** a versioned scenario file, clock configuration,
  seed, stable event order and pin-event scheduler. Reuse scenario/SimulationTest
  mechanisms for setup and precomputation; it is test equipment rather than
  a new behavioral peripheral.
- **Batch game runner and captures:** ELF + machine config + script → bounded
  execution, device trace, frame snapshots/hashes, final state and provenance.
  Frame captures have an epoch, sequence, virtual timestamp, geometry, pixel
  format and known-bit information. A run has explicit instruction/cycle/
  event/frame/resource limits and a termination reason.
- **Guest platform library:** RV32IM startup/linker script, stack/BSS and
  small MMIO/input/timer/pixel/text/PRESENT APIs, deterministic software PRNG
  and narrowly required runtime helpers.
- **Observation capabilities and adapters:** frame/console read-only access
  and a common external-input boundary, independent of concrete fidelity.
  Recorded viewing first; live transport adapters later.

The existing `RV32IElfImage` parses a restricted RISC-V ELF32 executable and
does not establish that its instructions use only a chosen ISA. Reuse its
format/loading checks, and add scoped build/artifact ISA validation for the
new programs. Merely linking or loading an ELF is not proof of RV32IM-only
code or successful simulated execution.

### Predetermined-I/O contract

Scripts supply **external stimuli**. The CPU executes game rules and writes
pixels/output through the real simulated interfaces. Expected device writes,
console text, frames and game states are assertions, not injected answers.
Predictable read results still come from device state, not a host shortcut
that feeds canned values to the CPU's memory-response port.

Canonical input events contain a 64-bit virtual tick, stable sequence, device
identity and payload. A cycle/phase notation can be resolved once against
the recorded clock configuration. Drive pins early enough to meet setup/hold;
specify same-boundary input, read, POP and reset precedence. The script's
ordering must not depend on host time, machine speed or execution quantum.
Assert expected read/output effects separately. Changing a clock or device
configuration creates a new resolved run rather than silently promising
the same timing trace.

Start with button transitions, reset and fixed seeds. Record console and
display output; an explicit bounded console-sink acceptance policy makes
backpressure deterministic. Pointer/serial RX/network input are optional
future script event types, not first-release requirements.

Batch runs can take longer than the virtual game they compute. Their output
is still valid recorded execution; they do not need real-time scheduling.
Use current recorded playback and add frame-at-time viewing. Trace capture
is configurable: retain full wire history for small structural scenarios,
and bounded device/retirement/frame evidence for longer games. Do not require
an unbounded gate-history trace for every game. History seeking remains
observation, not a resumable checkpoint.

### Roadmap with completion gates

| Phase | Deliverables | Required evidence before the next dependent phase |
| --- | --- | --- |
| 0 — Contract and compatibility baseline | Freeze new identities/pins, M latency/ownership, packet reuse, MMIO map/side effects, input/capture formats and game build configuration. Inventory current worktree, sources, profiles, layouts and old scenario outputs. | Existing RV32I decode/core, ALU, storage, memory/cache/arbiter and playback behavior have a reproducible baseline with code/build provenance. New manifests cannot accidentally select M for an I contract. |
| 1 — M semantics and structural arithmetic | Independent M reference and compiler smoke ELF; small multiply/divide steps, then iterative 32-bit units and `MulDivUnit32`. Add compact forms only after lower-level evidence. | All eight instructions covered; exhaustive small arithmetic, 32-bit boundary/random comparisons, signed/high products, divide-by-zero/overflow, reset/cancel/hold and timing. RV32I compiler/build paths unchanged. |
| 2 — RV32IM core | New decode, decode/execute stages, coordinator and five-stage family; reuse compatible packets and other pipeline blocks. Tiny assembly/C arithmetic programs before game software. | Structural/behavioral and independent-oracle agreement, dependent chains, forwarding, mixed memory/M waits, faults/flushes, reset/enable, x0 and exactly-once commit. Old RV32I still rejects M and matches baseline. Separate certified M operating clock. |
| 3 — Device foundation | Router/error target/fetch guard/register frontend, counter/FIFO, console/timer/buttons, tiny display storage and selector/PRESENT scenarios. | Protocol and device scenarios cover masks, invalid/X/Z fields, stalls, stable sampled reads, route ownership, FIFO races/overflow and reset. Device addresses bypass every cache. No speculative device side effects. |
| 4 — Recorded platform bring-up | `RV32IMDeviceSystem`, scalable video memory, guest library, scripted stimulus, batch runner and frame/console evidence. Begin hello-console → timer/button echo → color/pixel patterns → scripted moving cursor. | Guest-only MMIO execution and independent reference checks; ordered pixel writes → PRESENT snapshot; repeatable input/output hashes. Native/WASM parity on the same configuration, plus the long-time prerequisite below. Existing viewer scenarios still load/scrub. |
| 5 — Scripted games | Proposed sequence: 2048 → Sokoban → Snake → Pong → Tetris, with fixed seeds/levels and predetermined input sequences. Add game selection/build manifests and recorded viewing. | Per-game assertions for legal moves, collision/scoring, reset/restart and terminal conditions; frame hashes and final guest-state agreement. Snake taps, Pong two-player input and Tetris rotation/line-clear cases are explicit. No interactive runtime is needed. |
| 6 — Milestone acceptance | Run the old regression set plus new component/timing/core/device/game coverage; compare fidelity/native/WASM results; benchmark the integrated machine. | Publish code/config/script hashes, passing/failing/unrun checks, memory and event costs, cycles/update, host updates/s and capture overhead. Recorded game suite works with the new ISA/device platform while existing contracts/playback retain their baseline behavior. |
| 7 — Interactive simulation | Add persistent single-owner native/WASM sessions, bounded work quanta, queued live input, pause/reset, current-state observers, deterministic replay and optional wall-clock pacing. Feed the same device input boundary used by scripts. | Scripted/live-replay equivalence, bounded memory, input-to-pixel latency, stable pause/reset, responsive UI and explicit overload/lag behavior. Retain recorded mode and all batch games as regressions. |
| 8 — Optional hardware/game expansion | Pointer input, glyph/tile display, fill/copy unit, tone output and raster scanout; broader strategy/exploration/projectile/platform games. | Each feature has a concrete game requirement, lower-level structure, contract/timing/equivalence proof and new measured system budget. GPU/OS work is a separate future scope. |

Within each phase, keep changes small enough to review: reference semantics;
one arithmetic slice/unit; decode/control; core assembly; one bus/device;
guest library; one game; frontend recording; performance evidence. A helper
refactor or engine time migration is its own change, with its own old-behavior
check before feature work is layered on top.

### M implementation boundaries

Implement `MUL`, `MULH`, `MULHSU`, `MULHU`, `DIV`, `DIVU`, `REM`, `REMU`.
The [ratified M 2.0 specification](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html)
defines signedness/high halves, quotient rounding, remainder sign and
divide-by-zero/signed-overflow results. Do not introduce an arithmetic trap
for the defined divide-by-zero case. RV64 word instructions are outside
RV32IM. Machine-mode privilege, interrupts, C, F/D and A remain separate.

Prefer iterative arithmetic for the first structural full-width design.
Freeze iteration count and launch/sign-correction/completion edges before
writing the compact implementation. Both fidelities preserve latency and
cancellation semantics; a compact host multiplication is not permission
to publish a one-cycle result. Represent wide products/remainders with
supported-width ports/registers; no project-wide `Pin<64>`/`Pin<33>` expansion
is required.

At launch, latch operation, forwarded operands and ownership once. Hold the
result until the pipeline accepts it. An older fault or reset cancels its
right to retire. EX waits must permit controlled draining of older packets
without repeating writes, stores, predictor updates or retirement. A blanket
stall that freezes an occupied WB packet while still asserting commit is
invalid. Define ENABLE/pause behavior for an in-flight arithmetic operation.
Test back-to-back M, M→branch/store, load→M, rd=rs1/rs2, rd=x0, terminal/
fault overlap and wrong-path M.

Use `-march=rv32im -mabi=ilp32` only in the new game/M build path. Recheck
the selected compiler multilib, linked helpers and final disassembly; exclude
compressed/CSR/float instructions unless separately implemented. Include
small explicit M programs even if a particular game optimizes to pure I.
New family clocks and setup/hold evidence are derived after integration;
the old 4,104 ps compact-I clock is not automatically an M-core certificate.

### Time, display and cache prerequisites

Retain the draft map above: low RAM, separate device register windows and
uncached video memory. Freeze exact offsets/masks/version in phase 0.
Begin with persistent 64×48 index-8 drawing memory and snapshot PRESENT;
phase 4 recorded captures use the same atomic capture boundary required by
the later live renderer. Reuse 4 KiB FF video storage with 3,072 visible bytes
and explicit geometry. No display DMA or second full-frame copy in simulated
hardware is assumed by this first contract.

**Long browser recordings need 64-bit simulation time even before live
interaction.** Native 64-bit runs can bring up early devices, but the current
wasm32 `size_t` time span at 1 ps is only about 4.29 ms. Move time hardening
from the deferred interactive stage to a separate prerequisite before phase
4's long native/WASM captures. Audit event times, histories, component delay
arithmetic, gate timing and bindings. Use an explicit time type with checked
addition/conversion; preserve existing APIs where values remain representable
and specify an exact wide-time representation for JS. Old saved playback
data must still load. This is a cross-cutting compatibility change with an
independent regression gate, not a reason to implement live sessions early.
Any earlier browser test exceeding that time range also depends on this fix.

Keep the earlier burst restriction: `TwoPortMemoryArbiter` counts all
expected response beats and does not infer an early end from error status.
Reject forbidden device bursts before arbitration. Routing ordinary CPU
reads/writes to devices requires no new external memory-protocol signal.
Any future burst/DMA extension needs a separately agreed error contract.

Use two configurations under the same device ABI: a compact FF baseline for
throughput/long games and existing cache/shared-memory arrangements for
integration and detailed inspection. Add a small cached game/system smoke
case before claiming cache compatibility; do not require every long game
to run at an interactive rate through structural cache control. Recheck
FENCE and write-to-PRESENT visibility for every supported topology.

### Acceptance evidence and later games

Planned regressions cover unchanged public pin/catalog schemas, old profile
manifests/layout paths, original ALU and three I-core scenarios/programs,
oracle results, memory/arbiter/cache behavior and recorded viewer scenarios.
Generate full timing tables with `--clocked`, rebuild and verify; examine old
entries for unintended changes. Build serially, use focused checks during
each change, and run the existing registered native suite plus applicable
WASM/browser/site checks before declaring the milestone complete. Distinguish
these from optional long benchmark campaigns, and record every omitted gate.
Add each new logical scenario through TestRegistry, CTest and frontend
metadata; publish implemented component references and generated examples
in the same implementation change. Require zero setup violations at each
declared supported system clock and report hold/settling coverage separately.

For each scripted game retain its ELF/toolchain/config/input provenance,
device/retirement checks, frames and final guest-state fingerprints. Compare
the same configuration across repetitions, fidelities, native/WASM and
execution quantum sizes. Detect resource limits and failures explicitly;
do not treat a bounded run that exhausted its instruction budget as success.
Record sampled timer/input values to diagnose divergence, and also check
those samples against independently specified device behavior.

The first five games provide merge/push puzzles, grid motion, two-player
collision and falling-block workloads. Later candidates remain Minesweeper,
Reversi/Connect Four, a tiny dungeon, Breakout, Space Invaders, Asteroids and
a small platformer. Choose original small implementations or pin/review a
source port and assets; the previously inspected upstream repositories are
references, not finished ports. Pointer/tile/audio/scanout hardware follows
a selected game's requirement.

Performance acceptance first means reproducible, bounded and correct batch
execution. There is no real-time FPS requirement in phases 0–6. Measure M
unit events, integrated cycles/update, host throughput and recording cost;
the earlier RV32I headless measurements do not certify the new M/device
machine. Only phase 7 claims delivered live FPS and input latency.

### Verification of this roadmap update

Read current module interfaces/assembly, decoder/control/packet layouts,
oracle/ELF loader, simulator/test/visualizer paths, component/test guides,
public contracts and the primary M specification. The change is confined
to the open device/future records, STATUS and CHANGELOG. No product code,
tests, scenarios, toolchain flags or generated timing tables were changed.
No build, test, compiler probe, FPS rerun or published-site check was run.
Implementation has not begun. Planning-document whitespace checks passed.


## 2026-10-21 — Phase 0–3 implementation underway

The owner authorized implementation and emphasized host OOM risk. Work uses
`.worktrees/rv32im-phase3`, branch `rv32im-phase3`, based on `60acda2`. This
section replaces the preceding planning-only implementation status. The
[implementation record](2026-10-20-rv32im-phase0-3-implementation.md) is the
current source of changes and verification evidence. The adopted full
roadmap and later game/live phase order above remain in effect.

Structural M arithmetic/core and all 11 bus/device/display foundation designs
are implemented, with independent M semantics, a compiler-produced C fixture,
registered checks, recorded aliases and public component references. Native
checks and three-profile standalone timing certificates passed before a
native/WASM construction-order revision; current verification continues.
The initial M coordinator conservatively holds occupied older packets and
suppresses commit during waits, while accepted memory transactions retain
ownership. Independent draining is deferred as an optimization. Whole new
component behavioral forms are not claimed.

No complete RV32IMDeviceSystem, guest MMIO library, scripted game runner,
frame capture or live session is implemented in phases 0–3. Their later gates,
including wide-time hardening before long WASM captures, remain required.


## 2026-10-21 — Additional acceptance requirements and interactive authorization

The owner added cache-style general device parameters, a larger-screen test,
and visual component abstractions for display and keyboard input to phase 6.
The owner authorized proceeding into phase 7 after phase 6 is complete. The
[parameter/visual record](2026-10-21-device-parameters-and-visuals.md) tracks
this work; the phase completion gates above remain in effect.

## 2026-10-22 — Interactive implementation notes (not implemented)

Phase 6 remains open. Preserve the batch path and its golden recordings.
After acceptance, introduce one persistent execution owner with bounded event
quanta, queued transitions at the same safe clock boundaries, whole-clock
pause and explicit cold machine restart. Keep recorded-time observers
separate from current settled live observations. Retain the existing device
and FIFO contracts; configure queue capacity rather than inventing a parallel
host-only input device.

Factor shared execution/checking carefully; a resumable state machine or
C++20 coroutine may retain the current loop's state without copying its
hardware semantics. Verify local toolchain support before choosing. Live
capture should retain the latest immutable presentation with bounded memory,
while deterministic input logs permit replay through the batch checker.
Export explicit limits, queue/overflow state, cycles/s, delivered changed
frames and input-to-visible-output timing. UI responsiveness is a different
measurement from slow simulated-game response.

Provide native and WASM ownership adapters and a separate interactive UI
using the shared display/keyboard rendering. Timed games require live guest
loops; the finite scripted ELFs remain regressions. Compare the refactored
batch output and live-script replay against phase-6 exact frames/reports;
measure real games as well as a short synthetic interaction. Do not infer
live FPS from replay controls or a sparse larger-screen routing test.
