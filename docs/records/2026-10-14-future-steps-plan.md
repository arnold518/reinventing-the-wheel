# Future steps plan

Started: 2026-10-14. Status: open. Code state: `github-pages` at `448e4e3`, with pre-existing worktree changes.

## Summary

The owner named three long-term goals: interactive programs with device I/O and
graphics, a general-purpose GPU, and AI accelerators. The owner now prioritizes
cache work, then game programs, then AI accelerators; the general-purpose GPU
goal is unscheduled. Within cache work, the adopted order is a standalone
structural cache with a shared scalable-cache contract, measured timing and
one unified scalable RAM image, then scalable cache components. Current RV32I
execution, memory endpoints, fidelity selection, and the browser viewer
provide useful parts, but there is no guest-visible device platform or live
game loop yet. The research and earlier candidate sequences below are dated
proposals; the [cache system plan](2026-10-14-cache-system-plan.md) holds the
current three-stage order and proposed acceptance gates. The
owner also opened an ISA-expansion decision for the pre-game platform; no
extension has been selected. A researched program shortlist below identifies
source candidates, but the owner has not selected the first game.

## 2026-10-14 — Research

### Current implementation boundary

- `src/simulator/Simulator.cpp` and `include/simulator/Event.hpp` implement
  four-state, time-ordered event execution. `ComponentFamily`, `BuildProfile`,
  and the catalog select structural or behavioral parts; the lower-level-first
  rule in `AGENTS.md` applies to each new device, GPU block, and accelerator.
- `include/modules/rv32i/RV32IMemorySystem.hpp`,
  `src/tests/RV32IProgramRoot.cpp`, and `src/analysis/RV32IExperiment.cpp`
  connect a core to separate instruction and data endpoints. The public
  `site-docs/rv32i/4.1/machine.md` describes 256 KiB default memories and no
  privilege modes, interrupts, or resumable trap handler; the system-instruction
  page says MMIO support is not established. A search of
  `include/modules`, `src/modules`, `include/rv32i`, and `src/rv32i` found no
  guest-visible framebuffer, input device, address decoder, or DMA master.
  This is an inventory of the current paths, not a proof that no experiment
  exists elsewhere in the worktree.
- `include/modules/memory/MemoryProtocol.hpp` already defines byte-addressed
  aligned-word REQ/RSP ports, byte write masks, and FF/SRAM/DRAM/ROM/flash
  endpoints. The port is single-outstanding (`site-docs/memory/3.2.md`). A
  device address map and arbitration are new contracts; the current memory
  endpoint is a useful building block, not a complete shared system bus.
- The present oracle uses `RV32IFunctionalMemory` for RAM reads and writes.
  MMIO verification will need an independent device-aware reference model as
  well as pin-level tests. `site-docs/rv32i/4.1/instructions/system.md` says
  FENCE currently has no pending or reordered access to constrain; adding DMA
  or concurrent masters makes memory/device ordering a new design question.
- Single-cycle and baseline cores require zero-wait FF responses; the
  advanced five-stage core handles delayed endpoints. Introducing a
  variable-latency device to every core would therefore require a new
  adapter or a changed core contract, not just a new address range.
- `RV32IElfImage` accepts restricted static bare-metal ELF32 images, and the
  compiled-workload port already exercises RV32I C and runtime libraries.
  `VisualizerSession` constructs a registered test, runs its scheduled
  scenario, and scrubs recorded timestamps; this is not a live input/render
  loop. `tests/CMakeLists.txt` currently registers 451 CTests (`ctest -N`),
  but no test was executed for this planning review.
- The open [engine-performance record](2026-10-13-engine-performance.md)
  measured roughly 3,700–13,400 simulated cycles/s for compact CPU cores and
  51 cycles/s for the gate-level advanced core, in a few bounded workloads at
  the then-current code state. These are host throughput measurements, not
  game frame rates or a GPU/AI performance forecast. Its optimization phases
  remain proposals.

### Goal 1: devices, graphics, and playable programs

The shortest path is a small bare-metal platform: route data-port addresses to
RAM or MMIO registers; define a deterministic cycle-based timer and button/key
state; expose framebuffer storage and a palette or simple pixel format; and
render completed frames in a live browser session. Polling is sufficient for
an initial game. Interrupts and a full OS become separate later goals. A tiny
structural register decoder, input latch, framebuffer slice, and scanout path
should establish each device's contract before a compact large-screen form.
Tile/sprite composition or a blitter can follow once software-drawn frames
work; those are graphics components with their own observable contracts, not
prerequisites for the first display.
The CPU must execute the game logic; a browser-only recreation would not test
the platform goal. A freestanding RV32I C startup, linker map, and small device
API are needed even though the Embench port already supplies a different
bare-metal harness. The toolchain conventions specify `-march=rv32i` and
`-mabi=ilp32` for this target
([RISC-V toolchain conventions](https://github.com/riscv-non-isa/riscv-toolchain-conventions/blob/main/src/toolchain-conventions.adoc)).

Pong or Snake can exercise timed movement, keys, and redraw with little game
state. Tetris adds collision, rotation, and denser drawing. 2048 is a good
turn-based board/input test, but [the original 2048 source](https://github.com/gabrielecirulli/2048)
is JavaScript; running its page in the browser would not be an RV32I port.
Small C implementations should have deterministic scripted-input tests,
frame hashes or pixel assertions, and a separate live-play check. Device
latency, input event ordering, and simulated versus host time need explicit
contracts.

DOOM can remain a later integration target. [Doomgeneric's porting API](https://github.com/ozkl/doomgeneric/blob/master/README.md)
requires frame presentation, key events, elapsed time, sleep, and a WAD data
source; sound is additional. The current default memory size, restricted ELF
environment, lack of asset/file service, and measured CPU throughput require
separate sizing and profiling before a playable-speed claim. The older
engine-performance record's DOOM instruction counts are estimates, not a
measured requirement. The RISC-V [privileged architecture](https://docs.riscv.org/reference/isa/priv/priv-intro.html)
is relevant if the goal grows into an OS with traps and interrupts; it is not
required to begin with a polling bare-metal game.

### Goal 2: general-purpose GPU

A compute GPU milestone needs an executable kernel contract: launch and thread
IDs, multiple active lanes, per-thread register state, divergent branches with
an active mask and reconvergence rule, global/local memory, and a correctness
oracle over kernels from different domains. A wide ALU or framebuffer alone
does not establish general-purpose execution. [Vortex](https://github.com/vortexgpgpu/vortex)
is a useful primary reference: it combines RISC-V-based SIMT hardware with a
compiler, driver/runtime, and a C++ simulator. Its present software and ISA
scope is far beyond this project's RV32I-only core; matching OpenCL or Vortex
compatibility would be a separate major software project.

The current single-outstanding, 32-bit CPU-oriented memory connection does not
yet support several independent lane/warp memory requests or DMA bandwidth.
An initial standalone GPU with local/global test memory could prove SIMT
semantics before CPU integration; shared-address-space, contention, cache,
atomics, and memory ordering would then need explicit contracts. The
benchmark envelope in `include/benchmark/Benchmark.hpp` can hold results,
but a GPU runner and kernel correctness checks do not yet exist.

### Goal 3: AI accelerators

A small integer dot-product or matrix engine has a narrower contract than a
general-purpose GPU. [Gemmini](https://github.com/ucb-bar/gemmini) illustrates
the main pieces: processing elements, a systolic array, scratchpad and
accumulator storage, DMA, and software-visible commands. [NVDLA's architecture](https://nvdla.org/hw/v1/hwarch.html)
shows how much more is needed for a broad neural-network inference stack:
convolution, activation, pooling, reshaping, buffers, and data movement.
These are research references, not libraries or components already present.

A representative 1-PE multiply-accumulate circuit and 2x2/4x4 array could
precede a compact larger array. Start with a specified integer format (for
example signed 8-bit inputs and 32-bit accumulation), overflow and reset
rules, and exact matrix-vector/matrix-multiply checks against an independent
software oracle. CPU-visible command registers can come after standalone
arithmetic correctness; scratchpad and DMA then reuse the device/interconnect
work. A full model importer, floating-point training engine, or framework
compatibility would greatly increase scope and is not implied by this first
milestone.

### Assessment and uncertainty

| Goal | Current fit | Main missing dependency | Candidate first proof |
| --- | --- | --- | --- |
| Small playable games | Closest: CPU, ELF, memory, browser UI exist | MMIO/address map, live input and frames, freestanding app harness | A CPU-executed Pong or Snake frame under scripted and live keys |
| AI accelerator | Arithmetic, registers and memory are reusable | Multiplier/PE slice, command interface, scratchpad/data movement | Exact small integer matrix multiply with structural/compact equivalence |
| General-purpose GPU | Simulator/component infrastructure is reusable | SIMT execution model, memory concurrency, compiler/runtime path | Two or more lanes executing divergent and memory-using kernels |

The order above is a feasibility assessment, not the owner's priority order.
The actual frame rate, useful framebuffer size, DMA throughput, and GPU/AI
simulation cost are unmeasured. Browser/WASM cost may differ from native runs.

## 2026-10-14 — Decisions

## 2026-10-14 — Proposed steps

Candidate sequence for discussion, with no phase approved yet:

1. Define a versioned bare-metal platform contract: guest address map,
   instruction/data RAM behavior, MMIO read/write widths and masks, error
   responses, reset, simulated time, and input event ordering. Preserve
   existing saved component contract IDs and fixed-program behavior.
2. Build and verify a minimal address decoder plus timer, keys and display
   components. Exercise lower-level device slices and their compact
   equivalents; add bus/device contract tests and a software-facing header.
3. Add a live run-control path beside recorded scenario scrubbing, with
   bounded stepping, pause/reset, deterministic input replay, frame snapshots,
   and native/WebAssembly parity. Measure host throughput before setting a
   frame-rate target.
4. Port one small C game to the simulated CPU, then expand through several
   contrasting games. Keep correctness/replay evidence distinct from whether
   live play feels responsive. Revisit DOOM only after asset, memory and
   performance budgets are measured.
5. Establish a shared-memory/interconnect contract that can eventually serve
   a CPU, display scanout, and accelerator DMA; begin with a small arbiter and
   explicit ordering before adding burst throughput or caches.
6. Prove an integer MAC and small systolic array, then command/scratchpad/DMA
   integration and end-to-end inference kernels. In parallel or later, specify
   a small SIMT kernel ISA/runtime, prove divergent multi-lane execution, and
   then add memory, synchronization, and broader toolchain support.

Choices still to make: first demo game and display format; polling versus a
planned interrupt milestone; whether the GPU should target a custom teaching
kernel interface or eventual OpenCL compatibility; which AI data type and
operation should define its first public contract; and the success threshold
for interactive host performance.

## 2026-10-14 — Unified memory and cache follow-up

### What is separate today

`rv32i::wireMemorySystem` in `src/modules/rv32i/RV32IMemorySystem.cpp`
wires the core's `IMEM_` and `DMEM_` transaction ports to two different
endpoints. Each compact endpoint owns its own `MemoryStorage`
(`src/modules/memory/BehavioralMemoryEndpoint.cpp`). The visual program root
and experiment runner construct both endpoints. They have independent copies
of addresses 0 through 256 KiB by default, not a shared physical RAM.

The ELF preparation path in `src/analysis/RV32IWorkloads.cpp` loads every
`PT_LOAD` segment into **both** stores, so compiled programs start with a
logically unified image. A later data store modifies only DMEM, however;
instruction fetch still reads the old IMEM copy. The instruction oracle has
both `step(state, one_memory)` and `step(state, imem, dmem)` overloads, while
the experiment runner and lockstep tests currently use separate memories.
`site-docs/rv32i/4.1/machine.md` accurately documents this Harvard testbench.

Unification is worth considering for one CPU-visible address map, code/data
sharing, device decode, and eventual display/GPU/AI DMA access. It is **not**
required to get the first game working: a data-port MMIO decoder could coexist
with separate IMEM/DMEM. It is also not equivalent to replacing both CPU
ports with one physical port. The single-cycle core can fetch while it reads
data; baseline pipeline IF and MEM can operate in the same cycle. Both require
zero-wait responses. A single arbitrated port can delay one request, which
only the advanced core currently handles. A unified backing RAM may still
expose two logical ports, or the advanced core alone may use an arbitrated
shared port at first. Existing component contract IDs and saved layouts should
not be renamed merely to change the surrounding testbench.

Copying one host `MemoryStorage` object into two current endpoints would not
model port conflicts, simultaneous read/write ordering, or device timing.
The lower-level-first path is a small shared multiport RAM or arbiter plus
address decoder, with a compact scalable form proven against its public
contract. Specify same-address simultaneous access, byte masks, reset,
faults, four-state values, and history. Existing ELF loading currently checks
the fixed 256-KiB limit inside `RV32IElfImage`, so a larger unified RAM also
needs a deliberate loader/capacity change. A single 256-KiB shared image can
hold less total distinct code-plus-data than two independent 256-KiB images.

Instruction visibility is a separate contract. The current core rejects
`FENCE.I` and has no instruction-cache invalidation. The [RISC-V Zifencei specification](https://docs.riscv.org/reference/isa/unpriv/zifencei.html)
uses `FENCE.I` to guarantee that a hart's later fetch sees earlier stores to
code. A unified uncached RAM might expose those stores immediately, but adding
an I-cache could change that behavior. Avoid claiming self-modifying-code
support until the visibility rule and any `FENCE.I` implementation are tested.

### Whether caches are worth building

Yes as **selectable** architectural components, after a backing memory and
address map have a stable contract. A small visible cache teaches tag/index
decode, valid/dirty state, line fills, replacement, masked stores, and
backpressure. A compact larger cache then permits useful program and working
set studies. Real systems commonly retain separate instruction and data L1
caches over shared outer memory; [Chipyard's memory-hierarchy documentation](https://chipyard.readthedocs.io/en/stable/Customization/Memory-Hierarchy.html)
provides one concrete example. A unified backing RAM therefore does not
require a unified L1 cache or a single CPU-facing port.

For this project, a small structural cache could start with a few short lines
and one outstanding miss. A read-only I-cache is the simpler first integration;
the advanced core can wait for a miss. A D-cache can begin with write-through
and a clearly specified allocation policy before a write-back variant adds
dirty eviction. A scalable behavioral implementation needs equivalence at
small matching configurations, bounded host memory, and timing contracts that
match the represented structural configuration. Current proven part-delay
tables cover specific component parameters; scaling cache size cannot silently
inherit an unrelated small-cache delay certificate.

Cacheable RAM, non-cacheable MMIO, and any display/DMA buffer need distinct
address attributes. A line fill must not prefetch side-effecting device
registers. The [RISC-V memory-model explanation](https://docs.riscv.org/reference/isa/unpriv/mm-eplan.html)
describes I/O regions as non-cacheable and says non-coherent DMA needs a
platform-specific synchronization mechanism. The current FENCE is a no-op
under the serial-memory policy (`site-docs/rv32i/4.1/instructions/system.md`).
Before adding concurrent DMA or write-back caches, choose a visible rule:
hardware coherence, uncached shared buffers, or explicit clean/invalidate
operations, and make FENCE ordering correspond to that rule.

No measured speedup follows from these components. The default FF endpoint
already returns in the request cycle; a miss inserts work and a hit still
executes cache logic in the host simulator. The cache may improve modeled
cycles against a slower backing memory while reducing host cycles per second.
Measure hit/miss counts, memory traffic, modeled cycles/ticks, host seconds,
and simulator events on controlled sequential, random, streaming, and game
workloads. Recompute clock certificates for each new structural hierarchy.

### Candidate integration order, pending decision

1. Freeze the new platform's address map and no-cache memory semantics. Keep
   the current two-endpoint program root as a regression target.
2. Prove one shared address space through two logical CPU ports, or use an
   arbitrated port on the advanced core first. Add unified-oracle checks,
   same-address and simultaneous-access tests, ELF/data/stack checks, and
   instruction/data port comparisons against the old uncached runs.
3. Add a small structural I-cache and D-cache as independent opt-in families;
   test hit, miss, eviction, masks, faults, reset, backpressure, X/Z, MMIO
   bypass, and code-store visibility. The advanced core is the initial miss
   integration target; fixed-cycle cores stay on their zero-wait path unless
   their contract is deliberately extended.
4. Add scalable compact forms and profile selection, then measured cache
   studies with slower RAM. Specify DMA/cache interaction before using shared
   buffers for graphics, GPU kernels, or AI accelerator transfers.

Open choices: unified backing for all three cores or initially the advanced
platform only; dual-port zero-wait RAM versus arbitration; cache write and
allocation policy; whether to add `FENCE.I` with the first I-cache; and how
DMA-visible buffers remain coherent. These are proposals, not decisions.

## 2026-10-14 — Clarification: CPU access paths and physical memory ports

The preceding discussion of two logical CPU ports describes the **current
CircuitSim core interfaces**, not an RV32I requirement or a claim that modern
CPUs have exactly two physical memory ports. A software-visible address space,
core request paths, cache ports, and backing-RAM ports are distinct choices.
The [RISC-V ISA introduction](https://docs.riscv.org/reference/isa/v20240411/unpriv/intro.html)
describes instruction fetch and explicit load/store accesses without
prescribing a physical port count. A modern example is the separate L1
instruction and data caches described in [Intel's cache documentation](https://edc.intel.com/content/www/br/pt/design/products/platforms/details/meteor-lake-u-p/core-ultra-processor-datasheet-volume-1-of-2/003/p-core-e-core-and-lp-e-core-level-1-and-level-2-caches/),
while [Chipyard](https://chipyard.readthedocs.io/en/stable/Customization/Memory-Hierarchy.html)
shows separate L1 caches connected to a shared outer memory hierarchy.

One core-facing memory port is a valid architecture. An arbiter could combine
instruction fetch and load/store requests into one shared memory interface;
when both need it simultaneously, one waits. A core that can stall for that
wait, such as the current advanced pipeline, is the least disruptive initial
place to try it. Alternatively, the existing two core-facing paths can feed
one shared address space through an arbiter or a multiported RAM. Preserving
simultaneous zero-wait fetch and data access for the current single-cycle and
baseline cores requires enough access bandwidth, or changes to their timing
and stall contracts. Neither option has been selected.

## 2026-10-14 — Scalable memory timing investigation

`MemoryContainer` is a size-configurable native device, not one timing model.
`createMemoryContainer` selects the behavioral implementation of FF, SRAM,
DRAM, ROM, or NOR flash without changing that device's native contract
(`src/modules/memory/MemoryContainer.cpp`). The REQ/RSP `scalable-endpoint`
adds a technology controller, or uses the fused behavioral endpoint. Its
transaction latency is therefore the controller and native-device latency,
including clock-edge delivery and backpressure, rather than an intrinsic
property of sparse storage.

| Model | Valid access at different addresses, with the same configuration and settled controls |
| --- | --- |
| FF | Reads use one address-independent timing rule within that device; writes capture on a rising edge. The FF endpoint is zero-wait in CPU cycles when its combinational path settles before the edge. |
| SRAM | A stable selected read or write runs the same timed phase sequence at every address; changing inputs restarts or cancels that sequence. |
| ROM | Native reads are asynchronous; its transaction controller waits a fixed settling interval for a valid read. |
| DRAM | Latency varies with open-row state, row changes, due refresh, and controller availability. Same-row accesses can skip precharge/activate; another row may require both. |
| NOR flash | Reads use one command sequence; program and erase have longer, distinct delays. Busy state and a prior command can delay acceptance. |

For the large FF model, "same timing at every address" does **not** mean
"same tick count at every configured capacity" or "all signal changes take
the same number of ticks." The catalog's `PartDelays` table distinguishes
address, enable, and clock paths. It is proven on small gate-level devices at
64 and 128 words and extrapolated by depth above 128 words
(`src/components/selection/ProvenPartDelays.cpp`); those large-depth delays
are labeled estimates, not physical measurements or gate-level proofs. A
direct `createMemoryContainer` call without catalog timing uses the native
behavioral device's own delay. Full transaction observations also depend on
when the requester drives and consumes the REQ/RSP handshake.

With `paged=true`, `MemoryStorage` allocates 4096-byte host pages only as
needed and uses a `std::map` of resident pages
(`src/modules/memory/MemoryStorage.cpp`). A page lookup or first-write
allocation can change host execution time or hit a
resource budget; it does not schedule extra simulated ticks. Thus an
untouched, initialized, near, or far FF word has no cache-like simulated
miss penalty. A cache in front of today's zero-wait FF endpoint would add
miss cost without a modeled slow-memory benefit. A slower backing device or
explicit latency model is needed for a meaningful modeled cache-speedup
study; compare modeled ticks and host seconds separately.

Verification: inspected the container factory, five native models, controller
sequencer, delay-table selection, sparse storage, public memory pages, and
existing timing/contract tests. The existing `build-cleanup` binary passed
`FFAlwaysReadyControllerTest`, `ScalableMemoryEndpointTest`,
`NativeMemoryDeviceContractTest`, and `MemoryEndpointTimingTest` (4/4).
No fresh build or per-address latency sweep was run, so the address-timing
statement is derived from the implementation, not a new measurement.

## 2026-10-14 — Preferred memory-merge design (proposal)

Today the program root wires instruction fetch and load/store to two
independent endpoint storage objects. Provisioning copies the ELF image to
both, but a later data store changes only the data copy. The proposed merge
keeps the core's two request paths while making RAM contents and the address
map shared. An arbiter accepts requests from fetch and load/store, forwards
one at a time to a RAM/MMIO address decoder, and returns each response to
its requester. If both paths request a single-ported backing at once, one
must wait; no second RAM copy is needed. A dual-ported RAM is another way to
serve both simultaneously but is not required for one address space.

For a first implementation, use the advanced pipeline, which already accepts
memory waits. Begin with a small structural two-requester arbiter and shared
RAM slice, then prove its compact large-memory form against the same
handshake and ordering contract. Keep the existing fixed-cycle testbenches
as regression targets until their zero-wait requirement is addressed by
stall support or sufficient RAM ports. MMIO can share the same address map
through the decoder. Separate optional I/D caches may later sit near the
core over this shared backing, as in common CPU hierarchies; neither cache
is needed for the initial merge. Specify simultaneous requests, response
routing, stores to code, and device ordering before calling it complete.

This is a recommendation for discussion, not an adopted design decision.

## 2026-10-14 — Sequencing the first program and memory work (proposal)

This refines the earlier candidate sequence; it does not adopt a milestone.
Define the guest address map and uncached MMIO contract first so software
addresses remain stable. Then deliver one small CPU-executed game with input,
timer, display, and a live run loop using the current separate instruction
and data memories. Ordinary statically linked code does not require stores
to become new instructions, so this first proof can use a data-side MMIO
decoder without a shared backing RAM.

After that first end-to-end program, build and test one shared RAM behind
the advanced core's two request paths. Do this before CPU/display DMA or
GPU/AI accelerator memory sharing, and before treating the platform as a
broad general-program target. The test should cover simultaneous requests,
response ownership, the ELF image, data loads/stores, code-store visibility,
and MMIO ordering. Retain the original two-memory program tests as a
regression path. This timing allows the game to expose actual memory and
software requirements before the interconnect is fixed.

Build caches after the uncached shared path and representative program
workloads exist. The current default FF endpoint responds in the same CPU
cycle, so a cache has no modeled slow-memory penalty to remove. Add a
slower backing configuration, then compare modeled cycles/ticks and host
seconds with and without small structural caches before scaling them.
Chipyard's [memory-hierarchy documentation](https://chipyard.readthedocs.io/en/stable/Customization/Memory-Hierarchy.html)
shows that separate optional L1 caches can sit over shared outer memory;
this is an architectural reference, not evidence of a speedup here.

## 2026-10-14 — Owner decision: memory and caches before games

The owner chose to finish the unified-memory and cache stage before
attempting games. This **replaces** the preceding proposal to build one game
on the two existing memories first. The stage should include a defined
address map, one shared backing memory, a small lower-level cache design,
its scalable equivalent, core integration, and meaningful correctness and
timing checks. Exact port arbitration, cache geometry/policy, and acceptance
criteria still need design; this decision sets order, not those contracts.

### RV32I and multiplication research

RV32I has no hardware multiply or divide instruction. The optional
[RISC-V M extension](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html)
adds them; its Zmmul subset adds multiplication without division. This does
not prevent RV32I software from computing products or quotients. GCC's
[libgcc documentation](https://gcc.gnu.org/onlinedocs/gccint/Libgcc.html)
describes helper routines for arithmetic not directly supported by the
target. The repository's pinned multilib builder uses
`-march=rv32i -mabi=ilp32` and links `libgcc`, `libc`, and `libm`
(`tools/rv32i-benchmarks/build-embench-multilib.py`).

As a concrete local check, the existing `matmult-int.elf` under
`build/rv32i-embench-multilib/artifacts/matmult-int/` has a build manifest
recording exit 0 with the RV32I multilib. `riscv-none-elf-nm` shows
`__mulsi3`, `__divsi3`, and related helper
symbols in that ELF. `riscv-none-elf-objdump` shows the matrix loop calling
`__mulsi3`, whose routine uses `add`, shifts, bit test, and branches rather
than a MUL instruction. This establishes a working RV32I software multiply
path in one built workload. It is not a fresh simulation run or a game
performance measurement; the full Embench completion evidence is historical
and documented separately.

Simple integer games such as Pong, Snake, 2048, or Tetris are functionally
possible with RV32I and appropriate software helpers. They still require
the guest I/O platform, bare-metal port, memory budget, and enough host
simulation throughput for usable interaction. Hardware multiplication may
reduce instruction count for arithmetic-heavy drawing, physics, or later
DOOM-style fixed-point work; choose M or Zmmul only after representative
workload profiling. If added, follow the lower-level-first rule with a
representative multiplier slice, structural implementation, and equivalence
tests before a scalable behavioral form.

## 2026-10-14 — Reconsider ISA expansion before games

The owner clarified that the question is whether to **expand the CPU ISA**
to support useful operations, not merely whether software can emulate them.
This replaces the preceding suggestion to defer the M/Zmmul decision until
after game profiling. ISA selection belongs in the pre-game platform plan,
while the owner-selected memory/cache-first order remains in effect.

The standard [RISC-V M extension](https://docs.riscv.org/reference/isa/v20240411/unpriv/m-st-ext.html)
adds `MUL`, three high-half multiply variants, signed/unsigned `DIV`, and
signed/unsigned `REM`. `Zmmul` is the standardized multiply-only subset,
using the same encodings. Integer-only games can still run on RV32I, but
hardware arithmetic may reduce their instruction count and may also provide
a reusable lower-level multiplier design for later accelerators. Whether it
improves **host** simulation speed depends on the implementation and must be
measured separately from guest instruction count.

| Candidate | Software and hardware consequence |
| --- | --- |
| RV32I | Keep the present cores and use arithmetic library calls; functional baseline, potentially many guest instructions per multiply/divide. |
| RV32I + Zmmul | Implement four RV32 multiply operations; retain software division. Smaller arithmetic circuit scope, but requires a toolchain that actually emits `MUL` for C multiplication and compatible runtime libraries. |
| RV32IM | Implement all eight RV32 M operations, including divide and remainder. Directly supported by the pinned toolchain's code generation and multilib, but adds a divider and its timing/control contract. |

Local pinned-toolchain check: `xpack-riscv-none-elf-gcc-12.4.0-2` at `-O2`
compiled `int mul(int a,int b){return a*b;}` and a matching division function.
With `-march=rv32i -mabi=ilp32`, it emitted calls to `__mulsi3` and
`__divsi3`. With `-march=rv32izmmul`, it **also** emitted those calls;
`-print-multi-directory` selected `.` rather than a dedicated RV32I/Zmmul
runtime. A tiny Zmmul ELF linked, but its pulled-in default `__mulsi3`
contains 16-bit compressed instructions, which the current core cannot
execute. Recognizing and linking the `zmmul` architecture string therefore
does **not** establish a usable Zmmul C program toolchain here. With
`-march=rv32im`, the compiler emitted `mul` and `div`, selected the
`rv32im/ilp32` multilib, and linked a tiny multiplication ELF whose
disassembly contains `mul`. This is a compile/link/disassembly check, not a
simulation or game check. A different compiler build may make Zmmul practical.

Current decode accepts only RV32I R-type funct7 values `0x00` and `0x20`;
M instructions use `0x01` and are illegal today. The ALU contract has no
multiply/divide operation. An adopted extension needs independent oracle
semantics, decode and control, a lower-level arithmetic slice and structural
design, timing analysis, pipeline result/hold handling, focused arithmetic
edge cases, compiled-program tests, and matching public ISA documentation.
The advanced core can be the first integration target; extending the other
cores is a separate compatibility choice. Existing RV32I programs must keep
working. Prefer standardized scalar extensions for ordinary game arithmetic:
custom opcodes would also require a deliberate compiler and software contract.

Provisional recommendation: evaluate **RV32IM first** for the first
compiled-game target because the pinned toolchain already emits M operations
and selects an RV32IM multilib. Keep Zmmul as a smaller-hardware alternative if
its C code generation and runtime are verified or upgraded. During the
memory/cache stage, define an arithmetic-unit timing contract; then use a
small structural multiplier/divider slice and representative drawing/game
kernels to decide whether full M is worth implementing before the first
game. No ISA extension or ISA implementation milestone has been adopted yet.
Floating point, compressed instructions, atomics, and privilege
support should each be considered against an actual program requirement,
not assumed necessary for Pong, Snake, 2048, or Tetris.

## 2026-10-14 — Candidate programs and available source

The suggested order **after** the owner-selected unified-memory/cache stage
and the pre-game ISA decision is 2048, Snake, Pong, Tetris, then a busier
sprite game. This is a planning recommendation, not an adopted game order.
The first target should prove a guest program reading input and writing a
visible display; each later target should exercise one more platform need.

| Candidate | Source available | Why attempt it; porting boundary |
| --- | --- | --- |
| **2048, first interactive target** | [Original JavaScript](https://github.com/gabrielecirulli/2048) and [MIT-licensed C console clone](https://github.com/mevdschee/2048.c/blob/main/2048.c) | Turn-based board, keys, deterministic moves and redraw; no real-time frame deadline. The C clone's board/move functions are a promising starting point, but its Unix terminal, timing, signal and ANSI code must be replaced with guest input/display calls. The original JS is a rules/appearance reference, not RV32 guest code. |
| **Snake** | [Jason Pepas's MIT-licensed C/SDL 1.2 source](https://gist.github.com/cellularmitosis/6e54ea6d0965bff16ac947c19bc06d3c) | First continuous loop: movement timer, directional input, collision and grid display. Its ring-buffer state and `init`/`update`/`draw` split are useful; SDL, host time, allocation and random seeding need porting or bounded replacements. |
| **Pong** | [Tennis, a C/SDL Pong clone](https://github.com/catsocks/tennis-sdl), mostly CC0 with [third-party exceptions](https://github.com/catsocks/tennis-sdl/blob/master/THIRD_PARTY_NOTICES.md) | Exercises paced frames, moving paddles, ball collision and optional simple AI. Use the game rules/state as reference; its SDL, sound, touch and assets are beyond the first guest platform. |
| **Tetris** | [raylib classic Tetris C source](https://github.com/raysan5/raylib-games/blob/master/classics/src/tetris.c), under the repository's [zlib license](https://github.com/raysan5/raylib-games/blob/master/LICENSE); another [MIT C/SDL2 implementation](https://github.com/olzhasar/sdl-tetris) exists | Adds falling-piece timing, rotation, row deletion, denser rendering, and score text. Both versions depend on host graphics libraries; keep their logic separate from a new minimal guest renderer. |
| **Space Invaders or Asteroids, later** | [Space Invaders C source](https://github.com/raysan5/raylib-games/blob/master/classics/src/space_invaders.c) and [Asteroids C source](https://github.com/raysan5/raylib-games/blob/master/classics/src/asteroids.c), both in the zlib-licensed raylib classics collection | Multiple sprites/projectiles and collision stress CPU drawing and memory traffic. Asteroids also uses trigonometry and floating point in its reference code, so Space Invaders may be the easier first sprite target. Confirm the chosen arithmetic path before committing. |
| **Mandelbrot, non-game diagnostic** | [MIT-licensed C generator](https://people.sc.fsu.edu/~jburkardt/c_src/mandelbrot/mandelbrot.html) | Produces a deterministic image and stresses multiplication, loops, framebuffer writes and cache behavior without input timing. Its PPM output and floating-point arithmetic need a guest output path or fixed-point adaptation; compare pixels against a matching host reference. |
| **DOOM, later integration target** | [Doomgeneric C port](https://github.com/ozkl/doomgeneric), GPL-2.0, with a documented small platform API | Existing source lowers the porting burden but still needs substantially more memory, game data/WAD, filesystem or asset provision, input/timing/frame presentation and usable simulator throughput. It is not a near-term acceptance test. |

Availability of source is not equivalent to an executable RV32IM image. The
initial program port should retain the chosen upstream license/notice, pin a
source revision, and isolate game logic from a small guest interface for
buttons, monotonic simulated time or ticks, and drawing. Do not bring SDL,
raylib, Unix terminal calls, or browser JavaScript into the CPU binary. Build
and inspect the ELF against the selected ISA and memory map; exercise the
same game-state transitions on the host and guest; then test guest-driven
frames and real interactive input. The current 256 KiB default memory size
and measured core throughput make framebuffer format and real-time
playability explicit sizing/performance questions, not established facts.
No candidate was compiled, ported or benchmarked for this investigation.

## 2026-10-14 — Owner priority and cache planning start

The owner stated the working order as **caches, game programs, AI
accelerators**. In this record, “caches” includes the one-address-space
memory integration needed to use them. This confirms the earlier
cache-before-games choice and makes AI accelerators the next planned large
goal after games. The general-purpose GPU remains a long-term goal without
a position in this three-stage sequence; no decision to abandon it was made.

Cache work now has its own open [topic record](2026-10-14-cache-system-plan.md).
Its proposed gates cover a shared RAM/arbiter, small structural instruction
and data caches, scalable equivalent forms, and measured integration. The
microarchitecture, write policy, geometry, `FENCE.I` timing, and DMA
coherence remain proposals or open decisions. Game and AI accelerator plans
will be developed as their stages are taken up. No code, guest game, or cache
measurement was produced by this planning update.

## 2026-10-14 — Owner revision to the cache-work order

The owner now chooses **standalone structural cache → timed unified scalable
memory → scalable cache**. This replaces the order in “Candidate integration
order,” “Preferred memory-merge design,” and the initial cache record's C0–C5
milestones; those earlier sections remain dated research. Stage 1 defines one
REQ/RSP cache contract and proves a small visible cache against an existing
memory endpoint. Stage 2 measures expanded small memories against scalable
forms in simulator ticks, calibrates modeled timing where evidence requires
it, and connects the advanced core's two request paths to one scalable RAM
image. Stage 3 builds the compact cache under the same contract, proves
equivalence at shared small geometries and compares integrated programs.
The current [cache system plan](2026-10-14-cache-system-plan.md) has the
details and open design choices. No implementation or timing measurement was
performed for this revision.
