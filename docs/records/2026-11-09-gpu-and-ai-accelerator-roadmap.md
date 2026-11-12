# GPU and AI accelerator roadmap

Started: 2026-11-09. Status: open. Code state: `alpha` at `1ee0600`, with
another session's uncommitted games-machine memory changes in the tree
(not touched by this work).

## Summary

The owner's grand goal is a general-purpose GPU, with AI accelerators built
on the way. This record is the plan for that goal; it replaces the GPU and
AI sections of the [future steps plan](2026-10-14-future-steps-plan.md) as
the place where this work is planned. Part I is built and measured
([Part I implementation](2026-11-10-part1-accelerator-implementation.md),
site chapter 6); Part II is next.

Agreed path (2026-11-10, 14 stages, table in that day's section):
**Part I**, an integer TPU-style accelerator on the chapter-5 computer
(multiply-add cells, systolic array, DMA and bus work, the device running a
small MNIST network); **Part II**, floating point and Volta's tensor-core
unit; **Part III**, the Volta-shaped GPU (software foundation with a PTX
translator, SIMT core, warps, SM memory, full SM, several SMs with a host
runtime, per-thread program counters), then graphics.

Owner decisions so far (2026-11-10): compute first, then graphics; reduced
size with every real GPU component present; an NVIDIA-style SM with its
own SASS-like instruction set, running CUDA kernels compiled to PTX and
translated by our own PTX translator; Volta as the reference generation;
every size a parameter, generated from templates (warp size 4–64,
default 32; float formats by exponent and fraction bits). Three
experiment campaigns (renumbered 2026-11-12): the accelerator against the
CPU at the end of Part I; the cost and accuracy of floating point at the
end of Part II; CPU, accelerator and GPU at the end of Part III, compared
with published Volta figures (no NVIDIA GPU available). The Part II plan
was agreed on 2026-11-12 (that day's sections): floating point in
general (units, RV32F on the CPU as stage 5e, the tensor core), campaign
2 in four parts (cost, accuracy, tensor-core numerics, CPU floats), and
the page-template and rich-scenario rules for every new component. Next:
Part II, after the owner's decisions listed in the Part I record.

## 2026-11-09 — Starting point

What the current source already gives this goal, and what is missing.
Facts come from the files named; nothing was built or run except the
compiler check below.

| Need | What exists | What is missing |
| --- | --- | --- |
| Scalar arithmetic | Prefix adders, ALUs, `RV32IALU`, word gates of width 2–32 (`include/modules/composite`, `include/modules/basic/WordGate.hpp`) | Nothing for a lane's ALU |
| Multiplication | `MultiplyStep<W>` / `Multiplier<W>`: shift-and-add, one bit per edge, 32 step edges for W = 32 (`site-docs/system/5.1/arithmetic.md`) | A multiplier that finishes in one cycle or is pipelined; every MAC cell needs one each cycle |
| A whole processor | RV32IM five-stage core, instruction oracle, ELF loader, C games | Nothing for the host CPU |
| Machine built from data | `systems/*.json` → `ComputerSystem`; register-table device front ends; generated guest header (§ 5.5) | An accelerator is "just another device" once the bus can take a second master that does real work |
| Several masters | The bus takes any number of masters; three or more form a tree of two-port arbiters (`site-docs/system/5.2/bus.md`) | Fair N-way arbitration; the tree gives the last master half of all turns |
| Requests in flight | One per router, no transaction ID (`site-docs/system/5.2/router.md`, `site-docs/memory/transactions.md`) | Several outstanding requests, IDs or ordered queues; a GPU hides memory latency only with many requests in flight |
| Bus speed | 3 cycles per fetch on zero-wait memory: registered router request, arbiter not taking a same-cycle answer, core requester waiting a cycle ([main memory record](2026-11-08-games-machine-main-memory.md)) | These three serializations limit any bandwidth-bound unit |
| Data movement | Burst read (op 4) in endpoints and caches | No DMA engine; no master other than the CPU |
| Completion | Polling only; traps are terminal | Interrupts (the device review's P5, `traps=vector`) |
| Coherence | Caches have no coherence; the D-cache `INV_VALID` port is tied off; MMIO bypasses caches | A rule for DMA buffers (start: uncached) |
| Atomics | None (no RISC-V A) | Needed for GPU kernels that share counters; not for the accelerator |
| Floating point | None | Needed for most real GPU work; a full lower-level arc (adder, multiplier, FMA) |
| Wire width | Any width 1–64 bits per wire | Vector data must be per-lane wires, not one wide bus; this matches how SIMT lanes are drawn anyway |
| Simulation speed | About 1 µs CPU per event; compact cores thousands of cycles/s; gate-level cores tens (STATUS, [engine record](2026-10-26-engine-performance.md)) | Decides how big a unit can be at gate level (see "Simulation cost") |

## 2026-11-09 — Why the accelerator comes first

The owner's earlier order was caches → games → AI accelerators, with the GPU
unscheduled (future steps plan, 2026-10-14). Caches and games are done, so
the accelerator is next. It is also the better first step technically:

- A matrix unit has a narrow contract (int8 matrix in, int32 matrix out)
  and an exact oracle (integer matrix multiply). A GPU needs a whole
  execution model before its first test can pass.
- It forces the system work the GPU also needs (a second master, DMA,
  a scratchpad, higher bandwidth), but with one simple, regular traffic
  pattern instead of many threads.
- Its MAC cell becomes the GPU's tensor unit later, as in GPUs that added
  matrix units next to their SIMT lanes.

## 2026-11-09 — Proposed stages

Each stage follows the lower-level-first rule: the smallest structural
form first, an oracle and equivalence tests, then a compact form for scale;
a scenario; a site page; and a measurement. Each stage ends in something
that runs a real program, so it stands on its own if later stages wait.

### Stage A — Data movement (system prerequisite)

- **A1. Interconnect speed.** Decide whether to remove the three fetch
  serializations of the 2026-11-08 record (same-cycle answer through the
  arbiter, next request in the answer's cycle; keep the router's registered
  request or make it optional). Measure the games' CPI before and after.
  This changes core and bus timing, so recordings and certificates move.
- **A2. Several requests in flight.** A new port version with a small
  transaction ID, or in-order queues per target. Needed by stage E; the
  accelerator can live without it if its DMA uses bursts.
- **A3. Fair N-way arbiter.** Round-robin with N requesters, structural
  first (a 2-way and 4-way slice), replacing the unfair tree where asked.
- **A4. DMA engine.** A device with source, destination, length and start
  registers, and a master port; copies with burst reads. First target:
  uncached memory only. Tests: byte-exact copies, contention with the CPU,
  an error mid-copy.
- **A5. Banked scratchpad.** An SRAM split into banks so several words move
  per cycle; bank conflicts are visible. Reuses the clocked SRAM macro.
- **A6 (optional). Interrupts.** Machine-mode traps behind `traps=vector`
  as the device review planned (P5). Polling a status register is enough
  for stages B–D.

### Stage B — Parallel arithmetic

- **B1. Array multiplier.** A combinational multiplier (partial-product
  array, then a Wallace or Dadda tree feeding a prefix adder) at W = 4 and
  8, signed and unsigned. Checked against `Multiplier<W>` and an exhaustive
  sweep at W = 4 and 8. Delay table from the timing analyzer.
- **B2. Pipelined form** at W = 16/32 if needed by stage E (a one-cycle
  32-bit multiply would set a long clock).
- **B3. MAC cell.** int8 × int8 + int32 → int32, registered. Rules for
  overflow (wraps, as in two's complement) and reset fixed in its contract.
- **B4. Post-processing unit.** Requantize (multiply by a scale, shift,
  round, clamp to int8) and ReLU. Exact integer rules, oracle in C++.

### Stage C — AI accelerator v1: matrix unit

- **C1. Systolic array.** N × N MAC cells with input skew registers.
  Weight-stationary is proposed (weights stay in cells; activations flow
  in; partial sums flow down). Structural at 2 × 2 and 4 × 4; compact at
  8 × 8 and 16 × 16, proven equal at 4 × 4.
- **C2. The device.** Register table (command, addresses, sizes, status),
  scratchpad and accumulator memories, DMA in and out (stage A4), a small
  controller that tiles a matrix multiply. Built from a JSON description
  like every chapter-5 device; its C header is generated.
- **C3. Software.** A C driver on the RV32IM host; matrix multiply checked
  against an independent C++ oracle; the same multiply on the CPU alone
  (with `mul`) as the baseline.
- **C4. Measurement.** Cycles and simulated time for matrix multiply at
  several sizes; how much time is DMA, compute and CPU setup; utilization
  of the array. Same realistic-memory rule as the cache work: no tuning
  toward a good result.

### Stage D — AI accelerator v2: small real models

- Convolution through im2col (or a direct convolution controller),
  double-buffered scratchpad so DMA overlaps compute, a fused
  requantize/ReLU path.
- Workloads: a small int8 MLP and a small CNN on MNIST-sized inputs with
  weights quantized offline; the expected outputs come from a host
  reference. Fits the current 256 KiB RAM (an MLP with 784 × 64 weights is
  about 50 KB); the size limit is checked, not assumed.
- Optional: show the classified digit on the existing display.

### Stage E — SIMT core v1: one warp

- **Execution model first.** An independent functional SIMT model
  (threads × warps, active mask, reconvergence) written before any
  hardware, like the RV32I instruction oracle.
- **ISA.** RV32IM per thread, plus a few custom instructions in the
  `custom-0` opcode (`0x0b`): thread and warp IDs, set active mask, split
  and join for divergent branches (an IPDOM reconvergence stack), barrier,
  end of kernel. This is the [Vortex](https://github.com/vortexgpgpu/vortex)
  approach; the lane datapath reuses the RV32IM ALU and M unit contracts.
- **Hardware.** One warp of 2 lanes structural, 4–8 lanes compact: one
  shared fetch and decode, one register file per lane, an active mask, the
  reconvergence stack, a load/store unit that issues one lane's access at a
  time at first.
- **Tests.** Kernels with no divergence, with if/else divergence, with
  loops of different trip counts per lane, and memory-using kernels
  (vector add, reduction, matrix multiply), each against the SIMT oracle.

### Stage F — GPU core: latency hiding and memory

- Several warps and a warp scheduler (round-robin, then
  oldest-ready-first); a scoreboard so a warp waiting for memory does not
  block others. This is the central GPU idea and the main measurement:
  cycles against warps per core on memory-bound kernels.
- Shared memory per core (stage A5's banked scratchpad), with bank
  conflicts measured.
- A coalescer that merges a warp's accesses to one line; several requests
  in flight (stage A2); an L1 data cache reusing `Cache<W>`.
- Barriers across warps; atomic add/min/max/swap at the memory side,
  which also gives the CPU RISC-V A if wanted.

### Stage G — GPU system

- Two or more GPU cores on the bus, a command processor that the CPU
  starts through registers (kernel address, grid and block size,
  arguments), completion by status bit or interrupt.
- A tensor unit inside each core built from stage C's MAC cells, so
  matrix work in a kernel uses the accelerator hardware.
- Floating point: an FP32 adder, multiplier and fused multiply-add,
  structural first, as its own arc (owner decision 4). Optionally the same
  units give the CPU RV32F.
- Optional graphics: a rasterizer or simply kernels that draw into the
  existing display (Mandelbrot, a rotating cube), as the visible demo.

### Possible site chapters

Chapter 6 for stages A–D (data movement and the AI accelerator) and
chapter 7 for stages E–G (the GPU), following the chapter-5 pattern:
IO pins → behavior → implementation → examples → tests, with experiments
at the end of each chapter. A textbook anchor for chapter 7, as CS:APP is
for chapter 5: Aamodt, Fung and Rogers, *General-Purpose Graphics Processor
Architectures* (2018).

## 2026-11-09 — Simulation cost

Not measured for these units; these are the constraints that size the
plan. Native cost is about 1 µs CPU per event; gate-level cores run tens of
cycles per second and compact cores thousands (STATUS). So:

- Gate level is for representative slices: one MAC cell, a 2 × 2 or 4 × 4
  array, one warp of 2 lanes. A 4 × 4 int8 array is very roughly ten
  thousand gates (16 cells of an 8-bit array multiplier, a 32-bit adder and
  registers); this is an estimate to replace with the built count.
- Larger arrays and GPUs with 4–8 lanes × 4–8 warps run in compact form.
  Each extra lane or warp adds simulation work; there is no host
  parallelism. The 10 M events/s engine target becomes more valuable here.
- Register files grow fast: 32 registers × 32 bits per thread, so
  4 warps × 4 lanes is 16,384 bits per core, above the 4,096-cell limit
  of detailed memories. Lane register files are compact above the
  structural slice. RV32E-style 16-register lanes would halve this; that
  is a possible choice, not a proposal.
- Each stage's first measurement records events, host seconds and
  simulated cycles, so the next stage's size is chosen from evidence.

## 2026-11-09 — Toolchain check

Question: can kernels with custom GPU instructions be compiled by the
pinned toolchain without changes? Test in the session scratchpad (not
tracked), with `build/toolchains/xpack-riscv-none-elf-gcc-12.4.0-2`:

```c
static inline int lane_id(void){int r; __asm__ volatile(".insn r 0x0b, 0, 0, %0, x0, x0" : "=r"(r)); return r;}
static inline void tmc(int m){__asm__ volatile(".insn r 0x0b, 1, 0, x0, %0, x0" :: "r"(m));}
void kernel(int *a,int *b,int *c){int i=lane_id(); c[i]=a[i]*b[i]; tmc(0);}
```

`riscv-none-elf-gcc -march=rv32im -mabi=ilp32 -O2 -c` succeeded;
`objdump -d` shows `.insn 4, 0x078b` (the ID read into `a5`), ordinary
`lw`/`mul`/`sw`, and `.insn 4, 0x0007900b` (the mask write from `a5`). So
custom-0 instructions with register operands work from inline assembly;
the compiler allocates their registers normally. Not checked: linking a
whole kernel image, a runtime, or simulating anything.

## 2026-11-09 — Decisions for the owner

| # | Question | Recommendation |
| --- | --- | --- |
| 1 | Order: accelerator (stages A–D) before the GPU (E–G)? | Yes, matching the earlier order and reusing its parts |
| 2 | How does the accelerator attach: a memory-mapped device with DMA, or custom instructions inside the core (like Gemmini's)? | Device with DMA: it fits chapter 5's "system is data" and is reused by the GPU's launch path |
| 3 | GPU instruction set: RV32IM per thread plus a few custom instructions, or a new teaching ISA? | RV32IM plus custom-0: reuses the core's parts, oracle and the C compiler (checked above) |
| 4 | Number formats: int8 inputs with int32 sums first, floating point later as its own arc? | Yes; FP32 before claiming "general purpose" |
| 5 | Interconnect: fix the three fetch serializations and add requests in flight before the accelerator, or let accelerator v1 use bursts over today's bus? | Fix serializations first (A1, it also helps the games); requests in flight (A2) can wait for stage F |
| 6 | Completion: polling first, interrupts later? | Polling first |
| 7 | Simulation-speed work: continue the engine record's target in parallel? | Yes, but not as a gate for stages A–C |

## 2026-11-09 — Suggested first work

Stage B1–B3 (array multiplier and MAC cell): pure components, no system or
interconnect dependency, and none of the open decisions changes them. In
parallel, a short design note for A1/A4 once decisions 2 and 5 are made.

## 2026-11-09 — Owner: global goal, first decisions

The owner's intention is a realistic working GPU, SM level included. Agreed
so far:

- Compute first, then graphics (rasterizer, texture, pixel output) as a
  later stage of the same goal, no longer optional.
- Reduced size is acceptable as long as every component of a real GPU is
  present (command processor, SMs with warp schedulers, lanes, register
  file, shared memory and L1, shared L2, memory controllers).
- Open: which instruction set the GPU runs, and whether CUDA programs must
  run (discussed next). This may replace the stage E proposal of RV32IM
  plus custom instructions.

## 2026-11-10 — Owner: NVIDIA-style GPU running CUDA through PTX

Background given to the owner: CUDA compiles to PTX, NVIDIA's public,
stable virtual instruction set, and NVIDIA's `ptxas` (or the driver)
translates PTX into SASS, the per-generation machine code, whose encodings
and exact semantics are not published. AMD's GCN/RDNA/CDNA instruction
sets are fully published and an open LLVM backend emits them. Precedents:
GPGPU-Sim runs PTX; gem5 models AMD GPUs on their real instruction set;
Vortex uses RISC-V.

Decision: **option 1.** The GPU follows NVIDIA's documented SM
organization and has its own SASS-like machine instruction set. CUDA
kernels compile to PTX (with `clang`, which needs no NVIDIA driver or
GPU), and our own translator turns PTX into that machine code, as `ptxas`
does. The host side of a CUDA program (`cudaMalloc`, `cudaMemcpy`, kernel
launch) is a small runtime of our own on the RV32IM CPU.

Implementing a subset of AMD's real instruction set (option 2) may follow
later by owner choice. The RISC-V-plus-custom-instructions approach
(option 3, stage E's original ISA proposal and the `.insn` check above) is
no longer the plan; the check stays as dated evidence.

Not yet checked: that the local `clang` can emit PTX from a CUDA kernel.

## 2026-11-10 — Owner: Volta as the reference generation

Decision: the SM follows **Volta** (2017): four sub-partitions per SM,
each with its own warp scheduler, register-file slice, lanes and tensor
cores; combined L1 and shared memory; a shared L2. Reasons given: every
later NVIDIA SM keeps this layout, it is the first generation with tensor
cores (where the AI accelerator work lands), and it is the best publicly
measured (NVIDIA's Volta whitepaper, Jia et al., "Dissecting the NVIDIA
Volta GPU Architecture via Microbenchmarking", 2018; Accel-Sim, which is
validated against a real Volta card). Ampere/Hopper features (asynchronous
copies, thread-block clusters, distributed shared memory) can be added
later.

Branch reconvergence is staged: first the pre-Volta per-warp stack of
active masks, then Volta's per-thread program counters with explicit
convergence instructions as a separate step.

Confirmed by the owner (2026-11-10): every size is a parameter,
as widths are elsewhere. Warp size 4, 8, 16, 32 or 64 (default 32; the
active mask is one wire, within the 64-bit wire limit); SMs per GPU;
sub-partitions per SM (1–4); lanes per sub-partition, dividing the warp
size; warps per sub-partition; register-file, shared-memory, L1 and L2
sizes. CUDA's `warpSize` reports the built size. Warp-level CUDA code
(`__shfl_sync`, `__ballot_sync`, code written in steps of 32) is correct
only at 32 (and `__ballot_sync`'s 32-bit mask fails above 32), so each test
kernel declares the warp sizes it supports. Example: gate level 1 SM,
1 sub-partition, warp 4, 2 lanes; compact Volta-shaped, 4 sub-partitions,
warp 32.

## 2026-11-10 — Owner: both accelerators, and the stage order

Two places for an AI accelerator were explained: the **tensor core**
inside each SM, fed from the warp's registers by one warp instruction
(Volta: FP16 products, FP32 sums, 4×4×4 per tensor core per cycle), and a
**TPU-style device** beside the CPU (command queue, buffers, DMA, a
systolic array; Google's first TPU, NVDLA). The owner chose to build both,
the device first. This replaces the 2026-11-09 stages A–G.

Agreed order (the GPU software foundation stays at stage 7):

| # | Stage | Ends with |
| --- | --- | --- |
| | **Part I — TPU-style accelerator on the chapter-5 computer (integer)** | |
| 1 | One-cycle integer multiplier, int8 multiply-add cell, requantize and ReLU | Cells proven exact, gates first |
| 2 | Systolic array | 2×2 and 4×4 structural; 8×8 and 16×16 compact, proven equal |
| 3 | Data movement: remove the three fetch serializations, fair N-way arbiter, DMA engine, banked scratchpad | DMA beside the CPU, measured |
| 4 | Accelerator device: command queue, buffers, controller, C driver | A small int8 network classifies MNIST digits, checked and timed against the CPU |
| | **Part II — floating point and the tensor core** | |
| 5 | FP32 and FP16 add, multiply, fused multiply-add, compare, convert, IEEE rounding | Units proven exact |
| 6 | Tensor-core unit (Volta 4×4×4 FP16), standalone; optional float mode for the device | Ready for the SM |
| | **Part III — the GPU** | |
| 7 | Software foundation: instruction set, assembler, reference simulator, PTX translator, CUDA through `clang` | CUDA kernels correct on the reference simulator |
| 8 | Minimal SIMT core: 1 sub-partition, 1 warp, mask stack | Divergent kernels on hardware |
| 9 | Many warps: scheduler, scoreboard, banked register file | Latency hiding measured |
| 10 | SM memory: shared memory, coalescer, L1, several requests in flight | Memory-heavy kernels measured |
| 11 | Full SM: 4 sub-partitions, barriers, atomics, special-function unit, tensor cores | One Volta-shaped SM |
| 12 | GPU: several SMs, L2 slices, memory controllers, command processor, copy engine, host runtime | A whole CUDA program on the machine |
| 13 | Volta per-thread program counters | Volta's branch handling |
| 14 | Graphics | Its own plan |

Part I's DMA engine and command interface are the first forms of the
GPU's copy engine and command processor; stage 3's bus work and Part II's
arithmetic are reused by the GPU.

## 2026-11-10 — Owner: two experiment campaigns

Every component and stage gets its scenarios and tests as usual. A first
proposal of one or more comparison experiments per stage was rejected by
the owner as too many: as with § 4.8, which ran after all RV32I cores
existed, comparisons wait until a whole machine is built. Agreed instead:

**Campaign 1 — end of Part I: the accelerator against the CPU.**

- Machines: the RV32IM computer alone; the same computer with the
  accelerator.
- Workloads: matrix multiply at a few sizes; the small int8 MNIST network.
- Varied: array size (4×4, 8×8, 16×16); main memory (FF, SRAM, DRAM).
- Answers: the speedup, and what limits it (data movement against
  compute time).

**Campaign 2 — end of Part III: CPU, accelerator and GPU.**

- Machines: the CPU; the accelerator; the GPU at a few sizes (warps per
  sub-partition, number of SMs, tensor cores on or off).
- Workloads: campaign 1's network and matrix multiply, plus a few
  standard CUDA kernels (vector add, reduction, transpose, a stencil).
- Answers: the overall comparison; how added warps hide memory delay; how
  the GPU scales with SMs; what the tensor cores add.
- Realism check: our cache and memory latencies against published Volta
  measurements (Jia et al., 2018). The owner has no NVIDIA GPU, so no
  real-card runs for now.

Part II has no campaign of its own; floating point and the tensor core
are measured inside campaign 2. Area and energy estimates, roofline
charts and hardware performance counters were proposed and are left out
for now. As in the memory work, each campaign states its questions
before running, and nothing is tuned after seeing results.

## 2026-11-10 — Bus speed-up moved to its own record

While planning Part I, removing the three fetch waits of the chapter-5 bus
was listed under stage 3. It is a fix to the existing computer's bus, not
GPU work, and the GPU reaches its memory through its own crossbar. The
owner moved it to a separate record:
[bus interconnect speed-up](2026-11-10-bus-interconnect-speedup.md). Stage 3
keeps the DMA engine and, if the CPU and DMA share memory unevenly, a fair
round-robin arbiter. Campaign 1 runs on whatever bus exists then and states
its cycles per instruction.

The rest of the Part I plan (stage 1 array multiplier, multiply-add cell
and requantize unit; stage 2 weight-stationary systolic array; stage 4 a
TPU-like device with its own DMA, buffers, accumulators and a C driver,
running a 784 → 64 → 10 int8 MLP) is a proposal still under discussion.

## 2026-11-10 — Owner: Part I revisions (replaces the previous section's stage 3 change)

- **Bus speed-up back in Part I.** The owner put it back into stage 3; it
  stays tracked in its own [record](2026-11-10-bus-interconnect-speedup.md),
  whose level and option-or-default decisions are still open. Stage 3 is
  now: bus speed-up, DMA engine, fair round-robin arbiter if needed.
- **Multiplier like the adders.** As § 2.1 keeps `RippleCarryAdder<W>` and
  `ParallelPrefixAdder<W>` side by side, compares them, and every ALU uses
  the prefix adder: `Multiplier<W>` (one bit per edge) stays as the simple
  reference, the new array multiplier is built beside it, the page
  compares them, and the RV32IM core's multiply uses the array
  multiplier. Division stays iterative. This changes the core's
  multiply from 33 edges to about one, so with the bus speed-up every
  game's cycle count changes after Part I.
- **CNN included.** Besides the 784 → 64 → 10 MLP, a small CNN (3×3
  convolution with 8 filters → 2×2 max pool → 3×3 convolution with 16
  filters → pool → 10 outputs; about 200,000 multiply-adds per image,
  roughly 4× the MLP). It adds: strided (2-D window) DMA transfers so the
  device builds the convolution rows itself (im2col), and 2×2 max pooling
  in the requantize/ReLU path, as the first TPU had pooling in its
  activation path. The CPU-only baseline costs about 4× the MLP's
  simulation time per image, so it uses a few images.
- **Dataflow.** Weight-stationary was explained again (weights fixed in
  cells, inputs flowing right, sums flowing down, inputs skewed by row;
  output-stationary as the alternative; weight loading dominating a
  batch-1 MLP, as the first TPU observed). Awaiting the owner's choice.

## 2026-11-10 — Owner: weight-stationary; bus speed-up as the new default

- The systolic array is **weight-stationary**.
- The bus speed-up becomes the **new default**, not an option; its level
  is chosen after a source review in the
  [bus record](2026-11-10-bus-interconnect-speedup.md).

## 2026-11-10 — Part I plan (agreed)

Collects the Part I decisions of this day; it replaces the earlier Part I
proposals. Every component follows the lower-level-first path of
[adding a component](../guides/adding-a-component.md): gates first,
scenario and sweep tests, a compact form proven equal, delay tables, a
site page. Exact pins, catalog IDs and register maps are fixed at each
stage's start.

**Stage 1 — integer arithmetic.**

- Array multiplier, widths 4–32, signed and unsigned: AND-gate partial
  products (Baugh-Wooley for signed), a Dadda tree of the existing full
  and half adders, the existing prefix adder last. Built beside the
  iterative `Multiplier<W>` and compared on its page, as § 2.1 compares
  ripple and prefix adders.
- The RV32IM core's multiply uses the array multiplier (all four M
  multiplies); division stays iterative.
- int8 × int8 + int32 multiply-add cell for a weight-stationary array.
- Requantize unit: add bias, multiply by a scale, shift with rounding,
  clamp to int8, optional ReLU; 2×2 max pooling on the same path.

**Stage 2 — systolic array.** Weight-stationary N × N array of stage-1
cells with input skew and output de-skew registers. Gates at 2×2 and 4×4;
compact at 8×8 and 16×16, proven equal to the gates at 4×4.

**Stage 3 — data movement.**

- Bus speed-up, level 2, as the new default:
  [its record](2026-11-10-bus-interconnect-speedup.md). Followed by the
  reruns listed there.
- DMA engine: source, destination, length, start and status registers, a
  master port; 2-D strided (window) transfers for convolution rows.
  Usable as its own device and reused inside the accelerator.
- A fair round-robin arbiter if CPU and DMA share memory unevenly.

**Stage 4 — the accelerator device.**

- First-TPU shape at small size: register table and command queue; own
  DMA master; activation buffer (one 8-bit memory per column, within the
  64-bit wire limit); weight buffer; the array; int32 accumulators;
  requantize/ReLU/pool back into the activation buffer. Commands: load
  activations, load weights, multiply (accumulate or overwrite),
  activate, store; a done counter.
- A machine description in `systems/` with the device; C driver on the
  RV32IM; generated header. No caches by default, so DMA buffers need no
  coherence rule yet.
- An independent integer reference that the device matches bit for bit.
- Networks, trained and quantized offline with numpy (2.3.5 installed;
  PyTorch is not): a 784 → 64 → 10 MLP and a small CNN (3×3 conv, 8
  filters → pool → 3×3 conv, 16 filters → pool → 10 outputs). Weights and a
  few test images are embedded in the guest. Accuracy over the full test
  set is computed on the host from the bit-exact reference. Not yet
  checked: the MNIST download and its licence.

**Campaign 1** — as agreed: CPU alone against CPU with accelerator; matrix
multiply at a few sizes, the MLP and the CNN; array 4×4, 8×8, 16×16; FF,
SRAM, DRAM main memory. The CPU-only network runs take an estimated 10–30
minutes (MLP) and about four times that (CNN) per image in Compact, so they
use a few images.

**Order inside Part I.** Stage 1 and 2 have no system dependency. Stage 3
changes the chapter 3–5 defaults and is followed by reruns. Stage 4 needs
1–3.

## 2026-11-09 — Checks

- Read: `docs/STATUS.md`, the future steps, device architecture, Device
  I/O architecture, engine performance and games-machine memory records,
  the archived three-year roadmap, § 5 site pages and the headers named
  above.
- Ran: the compiler check above. No build, test or simulation was run;
  no product code changed.

## 2026-11-12 — Part II plan (agreed)

Proposed after Part I and accepted by the owner. It details stages 5 and 6
of the 2026-11-10 table; the lower-level-first path of
[adding a component](../guides/adding-a-component.md) applies to every unit.

What Part I gives it: `ArrayMultiplier<W>` (W = 4–32) for the 11×11 and
24×24 significand products, the prefix adders, the barrel shifter and the
requantizer's rounding and clamping. No floating-point part exists yet.

**Stage 5 — floating point.**

- 5a. An independent bit-exact float reference in integer C++, checked
  against the host processor (FP16 conversions exhaustively, FP32 by
  random and edge-case sweeps); it plays the instruction oracle's role.
- 5b. Building blocks, gates first: leading-zero counter, alignment
  shifter with a sticky bit, rounding unit, exponent logic.
- 5c. Units parameterised by format: adder, multiplier, fused
  multiply-add, compare/min/max, conversions (integer ↔ float,
  FP16 ↔ FP32). Adder and multiplier first, then the FMA, compared on the
  page as § 2.1 compares adders (a Volta lane is FMA-only).
- 5d. Pipelined forms with latencies from published Volta measurements
  (Jia et al. 2018).

**Stage 6 — the tensor core.**

- 6a. A four-element dot-product unit: four FP16 products into an FP32
  sum (one row of a tensor core).
- 6b. The 4×4×4 tensor core from 16 dot-product units, with a step
  interface shaped like Volta's `HMMA.884` (PTX `mma.m8n8k4`).

**Decisions (owner, 2026-11-12, all as recommended).**

1. The tensor core copies Volta's measured numerical behaviour (exact
   products, truncated alignment, one final rounding; to be confirmed
   against the published study, Fasi et al. 2021, before building), and
   the page shows its difference from step-by-step IEEE FMAs.
2. Full IEEE: all four PTX rounding modes, subnormals kept,
   flush-to-zero as a mode.
3. No hardware divide or square root; Volta computes them in software on
   the special-function unit (stage 11), and the PTX translator expands
   them.
4. RV32F for the CPU: yes, late (Part II's end or Part III), so campaign
   2's CPU baseline does not run software floats.
5. Formats: FP16 and FP32 only (Volta's); BF16 and FP64 later if wanted.
6. Browser sizes: one dot-product unit (or a 2×2×2 core) at gate level;
   the full 4×4×4 core Compact, proven equal (a gate-level full core is
   estimated at 200–400k gates).
7. No float mode for the Part I device (stage 6c dropped).
8. Site chapter 7, "Floating point and the tensor core"; the GPU becomes
   chapter 8.

**Experiments.** No campaign of Part II's own (as agreed 2026-11-10).
Its pages compare units in gates, delay and latency (adder and multiplier
against FMA, FP16 against FP32, integer against float multiply-add), and
its tests prove exactness against the references, including the tensor
core's drift from exact IEEE results on a few matrices. Campaign 2, at
the end of Part III, measures Part II's questions: what tensor cores add
over FP32 lanes on matrix multiply, and the same network in FP16/FP32 on
the GPU against int8 on the accelerator. Optional, not planned: an
FP16-against-int8 MNIST accuracy check at the end of Part II.

Not yet checked: the Volta tensor-core numerics study's exact findings;
that the local `clang` emits PTX (Part III).

## 2026-11-12 — Owner: a Part II campaign; sizes from templates

Replaces the "Experiments" paragraph of the previous section, and renumbers
the campaigns: campaign 1 stays at the end of Part I, a new **campaign 2**
runs at the end of Part II, and the former campaign 2 becomes **campaign
3** at the end of Part III. Campaign 2 takes the questions of the former
campaign 2 that need no GPU.

**Sizes come from templates.** As elsewhere (one builder taking the size,
a thin `template<size_t N>` wrapper per catalog size, e.g.
`MatrixUnitOf<N>` for `device.matrix-unit.N`, N = 4, 8, 16), every Part II
unit is one builder generated over its size parameters, not a class per
size:

- float units over the format, exponent bits E and fraction bits F
  (FP16 = E5 F10, FP32 = E8 F23), with families such as
  `float.fma.fp16`/`.fp32`; another format (BF16 = E8 F7) is a new
  instantiation plus its reference checks, not new hardware code;
- the dot-product unit over its length K, the tensor core over M × N × K
  (Volta 4 × 4 × 4);
- tests and page comparisons sweep the instantiated sizes; the float
  reference takes the same E and F.

The 64-bit wire limit holds for FP16 and FP32 (the FP32 FMA's widest
internal value is the 48-bit significand product); FP64 (106-bit product)
would need split words. The GPU's sizes in Part III (warp size, lanes,
sub-partitions, SMs) follow the same pattern.

**Campaign 2 — end of Part II: the cost and accuracy of floating point.**
Questions stated before running; nothing tuned after seeing results.

1. Cost: gates, critical-path delay, pipeline latency and multiply-adds
   per cycle per 1,000 gates for Part I's int8 `MacCell`, the FP16
   dot-product unit and the FP32 FMA; Part I's 4 × 4 systolic array
   against the 4 × 4 × 4 tensor core. Latencies are compared with
   published Volta figures (Jia et al. 2018).
2. Accuracy: the Part I MNIST MLP and CNN in FP32, in FP16 with FP32 sums
   (the tensor core's numerics) and in int8, over the full 10,000-image
   test set from the bit-exact references on the host; a few images run
   through the simulated tensor core (Compact) on a test bench to show
   the reference holds.
3. Tensor-core numerics: Volta-faithful against step-by-step IEEE FMAs,
   both against an exact result, on matrices of several sizes, value
   ranges and cancellation; the published Volta probes (Fasi et al.
   2021) reproduced as the realism check.
4. Pending owner (needs RV32F in Part II rather than later): the CPU's
   float speed, software floats on RV32IM against RV32F hardware, on
   float matrix multiply and the float MLP.

**Campaign 3 — end of Part III** keeps the GPU questions: tensor cores
against FP32 lanes inside the SM, latency hiding by warps, scaling with
SMs, the CUDA kernels, the networks on CPU, accelerator and GPU, and our
cache and memory latencies against published Volta measurements.

## 2026-11-12 — Owner: RV32F in Part II; campaign 2 in detail; page and scenario rules

Replaces item 4 ("pending owner") of the previous section's campaign 2,
and decision 4 ("RV32F late") of the Part II plan.

**Part II scope.** Part II is floating point in general: the units
(stage 5), the CPU using them (new stage **5e**, RV32F) and the tensor
core standalone (stage 6). Part III is the GPU (stages 7–14) and reuses
them: the FP32 FMA in every lane, the tensor cores in every sub-partition.
The special-function unit stays in Part III.

**Stage 5e — RV32F on the CPU** (an RV32IMF core as an option, not the
default, so earlier chapters' results and the long-game recordings stay
unchanged):

- the instruction oracle extended with F, using the stage-5a reference;
- a 32 × 32 float register file with three read ports (the fused
  multiply-adds read three registers);
- a float execute unit: the stage-5c FP32 FMA (add, subtract, multiply,
  fused ops), compare/min/max, sign injection, classify, conversions,
  integer ↔ float moves;
- an iterative divide and square-root unit with a start/done handshake as
  `MulDiv` has (RV32F requires them; the GPU still computes them in
  software);
- `fcsr` (rounding mode and the five sticky flags) and the minimal Zicsr
  instructions to reach it (the core has only `ECALL`/`EBREAK` today);
- decode, hazards and forwarding for float registers and the int ↔ float
  crossings, stalls for the multi-cycle FMA and divide;
- guests built twice from one C source: `-march=rv32im -mabi=ilp32`
  (software floats from libgcc) and `-march=rv32imf_zicsr -mabi=ilp32f`.
  Checked 2026-11-12: the xPack `riscv-none-elf-gcc` 12.4.0 has the
  `rv32imf_zicsr/ilp32f` libraries and emits `fmadd.s`; the soft-float
  build calls `__divsf3`.

Consequence for stage 5: every float unit outputs the IEEE exception
flags (RV32F needs them; the GPU ignores them).

**Campaign 2 — end of Part II, in detail.** Questions stated before
running; nothing tuned after seeing results; one run at a time under a
memory cap.

- **A. Hardware cost.** Units at each built size: int8 `MacCell`, FP16
  and FP32 FMA, FP32 adder plus multiplier against one FP32 FMA, the FP16
  dot-product unit, `SystolicArray<4>` against the 4 × 4 × 4 tensor core,
  RV32IM against RV32IMF. Measured: gates, critical-path delay, pipeline
  stages at our clock, multiply-adds per cycle per 1,000 gates. Questions:
  float against int8 multiply-add cost; does fusing pay off; tensor core
  against systolic array per gate. Realism: latencies against Jia et al.
  2018.
- **B. Accuracy.** Part I's MNIST MLP and CNN from the same trained float
  model (no per-format retuning) in FP32, FP16 with FP32 sums
  (Volta-faithful), FP16 with FP32 sums (IEEE step by step) and int8;
  full 10,000-image test set from the bit-exact references on the host;
  2–4 images through the simulated tensor core and FP32 FMA on a test
  bench, equal bit for bit. Question: what int8 loses against floats, and
  whether Volta's rounding matters.
- **C. Tensor-core numerics.** Sums of length 4–256 chained through
  4 × 4 × 4 steps (as kernels chain `HMMA`); uniform, normal,
  wide-exponent and near-cancelling values; error against the exact
  result for the Volta-faithful core, an FP32 IEEE FMA chain and FP16
  throughout. Realism: the published probes (Fasi et al. 2021) give their
  reported outcomes.
- **D. Hardware floats on the CPU.** The chapter-5 computer as in
  campaign 1 (Compact, SRAM, same bus and multiplier settings); software
  floats against RV32F on float matrix multiply at campaign 1's sizes,
  the float MLP (one image) and a divide/square-root kernel set (vector
  normalize, distances). Measured: cycles, instructions, cycles per
  multiply-add, divide/square-root wait. Set beside campaign 1's int8
  MLP: CPU alone 1,636,072 cycles, 16 × 16 accelerator 117,321. The
  soft-float MLP is the longest run (estimated 5–10× campaign 1's CPU
  MLP): a short slice is measured first.
- Output: tables in the Part II record and an experiments page closing
  site chapter 7 (as § 6.6 closes chapter 6).

**Rules for every new Part II component (owner).** Unchanged project
rules, restated so the work does not drift:

- **Page template.** Each component page follows the template of
  [adding a component](../guides/adding-a-component.md) § 7, as the
  chapter-6 pages do: component table; IO pins (names, directions,
  widths); behaviour; C++ example; unknown inputs and timing;
  structural and behavioral implementation; examples from recorded
  scenarios through `<!-- simulator-examples: … -->` markers (never typed
  rows) with an embedded simulator; tests. Internal blocks a reader can
  open in the default view and that have behaviour of their own get their
  pins and behaviour on the page; splits, joins, constants and
  pass-throughs do not.
- **Rich scenarios.** Every new component gets a scenario whose rows
  teach and test: every operation and mode (for float units each rounding
  mode), the special values (±0, ±∞, quiet and signalling NaN,
  subnormals, the largest and smallest normals, overflow and underflow
  boundaries, halfway ties), the exception flags, and readable labels
  (decimal values beside the bit fields). Checkpoints at meaningful
  points; the finest profile a browser handles, measured; a floorplan
  where the automatic placement does not show the structure; layouts
  checked with `tools/check-static-layouts.py`; WebAssembly equal to
  native, part names included; the [tests and scenarios
  guide](../guides/tests-and-scenarios.md) followed and kept current.
- Each part is built from one builder generated over its size parameters
  (previous section), with scenarios at the sizes the page shows.

