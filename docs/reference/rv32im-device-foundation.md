# RV32IM and device foundation contracts

This reference describes the additive RV32IM/device contracts. Public pin
schemas and component details are in [RV32IM](../../site-docs/rv32im/index.md)
and [device I/O](../../site-docs/device-io/index.md). The
[roadmap](../records/2026-10-19-device-io-games-plan.md#rv32im-scripted-roadmap)
and [implementation evidence](../records/2026-10-20-rv32im-phase0-3-implementation.md)
record implementation and validation separately from the maintained interfaces.
The [scripted milestone record](../records/2026-10-21-scripted-games-phase4-6-implementation.md)
tracks system/game acceptance through phase 6.

## ISA integration and fidelity

- RV32I instruction behavior, ISA-family pins, ALU opcode encodings, packets
  and enum values retain their meaning; an I core rejects M instructions.
  The IM CPU contract is `rv32im.core.educational-five-stage`. Datapath
  components use the exact configured word-family IDs; retired fixed-component
  IDs have no aliases. `RegisterFile32x32` is the architectural exception.
- The I and IM cores share private assembly blueprints. Decoder, executor
  and coordinator families are supplied explicitly. The ISA stage paths
  stay the same. Profile fingerprints and saved component/root keys reflect
  the actual configured contracts.
- Families expose structural forms. `MulDivUnit<W>` at W=32, the core,
  counter/FIFO, register frontend and routers also provide compact gate-list
  forms for their characterized configurations. Width variants are
  separate IDs. Compact lower-level adders, registers, existing pipeline
  parts and native memory remain selectable by BuildProfile. An explicit
  all-behavioral rule requires the existing unavailable-fidelity fallback;
  it selects a compact root only where that family provides one. Console, timer,
  button and display assemblies remain structural with selectable smaller parts.
- The independent M decoder/oracle delegates only I semantics to the
  existing oracle. Product arithmetic never calls it. Test host memory
  responders are verification equipment, not a product machine or device.

## Parameterized execution and transport

The [word-component reference](../../site-docs/components/word-components.md)
defines the new public C++ APIs. `ALU<W>` chooses extended or integer semantics
and ripple/prefix addition at construction; widths 8/16/32 use the same
operation/flag contract. Legacy ALU<8,extended,ripple>/ALU<32,integer,prefix> wrappers keep their own operation
maps, IDs and defaults. Shared recipes also back muxes, register files,
comparison, shifts, flags, result selection and operation decoding.

`Multiplier<W>`, `Divider<W>` and `MulDivUnit<W>` support 4/8/16/32 with W
enabled iterations after launch. The M core remains W=32 and retains its
shared pipeline blueprint. `RegisterFile<W,Count>` declares word width,
power-of-two count and the zero-register policy while retaining two read
ports and one write port.

Bus helpers expose 8/16/32-bit data and W/8 masks with 32-bit byte addresses.
Six named router regions accept validated base/size parameters; the default
map and old `video_bytes` variant key are retained. The current game
peripheral ABI remains 32-bit; narrower helpers do not imply an implemented
bridge or atomic fragmented access to its registers. Framebuffer dimensions,
capacity and queue depths are independent parameters. New width variants are
structural until their own behavioral equivalence/timing is established.

## M ownership and pipeline

M funct3 0..7 is MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU. The architectural
results follow the ratified [M 2.0 specification](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html).
The existing state/retirement observers remain usable.

Arithmetic accepts START when READY and ENABLE are high at a rising edge.
That launch is followed by 32 enabled iteration edges. DONE/result remain
owned until an enabled TAKE. ENABLE freezes in-flight work. CANCEL clears
ownership synchronously even when disabled; RST clears asynchronously.
Cancellation does not promise cleared payload pins. A new launch replaces
all payload needed for its result. READY is low while reset/cancel is high,
BUSY or an unconsumed completion exists.

The initial coordinator holds the pipeline during arithmetic and suppresses
COMMIT_ENABLE with packet movement. Occupied WB cannot repeat writes or
retirement. A memory request already accepted remains owned by its requester
and can complete into retained state. Older terminal/fault state overrides
the arithmetic hold and cancels its right to retire. Independent draining of
older WB during M is a possible later optimization, not current behavior.

## Device bus and side effects

The fixed map, offsets and status numbers are defined on the
[device index](../../site-docs/device-io/index.md). All new contracts have
catalog version 1. Router destinations use local offsets, with one held owner.
Read/write bursts are not introduced: only operation 0/1 is forwarded.

One CPU address space does not imply one memory component. RAM, pixel memory,
console FIFO, timer counter and button state are separate instances/structures.
The router removes each target's base address; high device bases and unmapped
gaps do not require a dense backing array spanning those addresses.

DeviceRegisterPort<32> has a registered accepted-request slot and the existing
MemoryResponseBuffer. REQ_READY depends only on reset and registered
occupancy. Request fields are captured once. Reads sample on the backend
execution edge, complete without BACK_READY, and retain the sampled result.
BACK_READY gates successful nonzero writes. An error or zero-mask write has
no backend side effect. A strobe is consumed at that execution edge once.

Validation priority is unknown operation/address, unsupported operation,
invalid required write mask, misalignment, then backend status. READ ignores
write data/mask. Register WRITE permits mask 15 or no-op mask 0 on valid writable commands. Reserved or
unknown command data is validated by its device. Pixel memory retains the
existing endpoint's byte-mask behavior.

Button HEAD reads are nondestructive; explicit POP writes remove events.
PRESENT is an explicit command, never a memory read. Timer reads retain the
value sampled by the hardware edge, not a value chosen later at retirement.
FIFO overflow may drop events but button levels still update. Console sink
acceptance is external and must have a deterministic policy for batch runs.

The instruction guard exposes only low RAM reads and has no device ports.
The data router exposes separate device/video destinations. The full system
must place all caches solely behind RAM_; this topology is not assembled by
phase 3. Do not claim integrated cache/MMIO behavior from standalone routing
checks.

## Display and reset

FramebufferDisplay defaults to 4x4 for structural inspection. Its catalog
parameters select positive width/height up to 4096 each, bounded to 256 KiB
of visible RGB332 bytes. Stride equals width, with no power-of-two constraint.
For example 64×48 uses 3072 visible bytes in 4096 storage bytes; 128×96 uses
12288 visible bytes in 16384 storage bytes. Explicit `storage_bytes` reserves
a power-of-two capacity covering the surface. The system's `video_bytes`
parameter propagates the same capacity into routing and capture. The endpoint is reused, including native behavioral
storage at scale; no second memory engine or framebuffer copy is introduced.

Tie both display clock/reset pairs to the global clock/reset. Reset discards
pending controller state and resets the frame sequence, while retaining
written pixel storage. Host image initialization is separate. PRESENT marks
the execution edge and increments the sequence; it does not capture bytes or
render a browser image. The PIXELS child owns MemoryStateView; the display root control bank does
not, so the inspector associates drawing storage with the pixel protocol.
Delegated root image helpers remain available. Tiny fully expanded memory
lacks a compact backing view, following the existing endpoint limitation.

Reset is coordinated across the bus and its targets; it is not a transaction
cancellation/retry protocol for independently resetting a router. A completed
write is not rolled back by reset. External input and handshake controls must
meet clock contracts and use binary values for the certified environment.

## Timing and test equipment

`RV32IMTimingTest` constructs all-structural M units and core and asks the
existing safe-clock analyzer for certificates under each timing profile.
The standalone boundary has RST=0 and binary changing inputs, including ENABLE, one tick
after falling edges and rising-edge output sampling. It is distinct from an
integrated memory/cache/device clock and from host throughput. No old I-core
clock is reused as an IM certificate.

`RV32IMFiveStageCoreTest` and `RV32IMCompiledProgramTest` compare every
retirement, architectural state and final memory against the independent
oracle, at a fixed 100-tick unit clock and the same response-input phase.
They count setup violations. Delayed host responders exercise protocol
ownership. The integrated device system instead uses simulated RAM/devices.

`src/tests/RV32IMPhase3Tests.cpp` holds bounded history-off sweeps and fault
cases; `RV32IMPhase3Scenarios.cpp` holds the smaller recorded component
examples. A recorded alias shares its sweep's logical owner. TestRegistry,
CTest and frontend metadata include the aliases. Scenarios run all available
root fidelities; MulDivUnit<32> now also requests its compact gate-list form. Exhaustive
small arithmetic also runs with fully structural and compact child choices.

## Recorded platform boundary

[RV32IMDeviceSystem](../../site-docs/rv32im/device-system.md) connects shared
RAM and separate device/video routes. The [batch runtime](../../site-docs/rv32im/recorded-games.md)
executes guest software, applies predetermined external input and checks ISA,
device samples, ordered writes and immutable PRESENT captures. Ten compiled
fixtures cover bring-up and the five games with additional acceptance cases.
Native/WASM, timing, game and compatibility acceptance is tracked in the open
milestone record; staged interfaces alone do not establish those passes.

Explicit 64-bit simulation time and exact wide JSON ticks support long WASM
recordings. The runtime turns off gate/register/memory histories and bounds
cycles, instructions, events, input, console, device samples and frames.
The additive recording viewer plays captured frames. Persistent live sessions,
wall-clock pacing and optional hardware remain phases 7–8.

## Device observations and capacities

FIFO `depth` and console/button `fifo_depth` accept 1–7 entries (default 4),
within the existing three-bit COUNT contract. The system forwards these as
`console_depth` and `button_depth`. Configured compact queues/routers derive
bounds from their own structural gates rather than the default variant.

`DeviceInspector` observes display pixels and input/queue/timer/console pins
at recorded time. It is shared by the native/Python and WASM viewers. Display
pixels use MemoryStateView history, or the tiny structural FF bank's existing
word histories. It schedules no events and cannot inject input. Device views
is a presentation toggle; the component's circuit and fidelity are unchanged.
Game recordings retain independently checked input snapshots with each frame
and applied transitions separately. Captured pixels are bounded to 4 MiB in
total (plus an equally sized known mask), regardless of configured geometry.
