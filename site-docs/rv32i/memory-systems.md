# § 4.7. Memory Systems

The [advanced pipeline](4.6/index.md) has two memory ports, `IMEM_` for
instruction fetches and `DMEM_` for loads and stores. Each is a
[transaction port](../memory/transactions.md), so it can reach memory in
several ways. This section shows five arrangements built from the parts of
chapter 3:

- [memory endpoints](../memory/controllers.md);
- the [two-port memory arbiter](../memory/controllers.md#353-twoportmemoryarbiterw);
- [caches](../memory/caches.md).

Arrangements B to D-ID are one [memory subsystem](../system/5.3.md)
(`MemorySubsystem`, child `MEMORY` of the program root) configured with
different caches: the core's `IMEM_` and `DMEM_` ports connect to its `I_` and
`D_` ports, and its parts are `INSTRUCTION_CACHE`, `DATA_CACHE`, `ARBITER`,
`SHARED_CACHE` and `RAM`, as configured. Each arrangement runs one program in
the simulator, so you can follow a request from the core to memory. Their measured speed over DRAM is in
[§ 4.8](experiments.md#memory-systems).

<!-- diagram: cache-topologies -->

| Arrangement | Connection | Parts between the core and memory |
| --- | --- | --- |
| [A](#a-separate-memories) | Each port has its own memory | none |
| [B](#b-one-shared-memory) | Both ports share one memory through the arbiter | arbiter |
| [C-ID](#c-id-first-level-caches) | B with an instruction cache and a data cache above the arbiter | two L1 caches, arbiter |
| [D-0](#d-0-a-shared-second-level-cache) | B with one shared cache below the arbiter | arbiter, L2 cache |
| [D-ID](#d-id-both-levels) | The two L1 caches above the arbiter and the L2 below it | two L1 caches, arbiter, L2 cache |

## The scenarios

All five scenarios run [program 21](4.2/programs/program-21.md) (a
sixteen-word array sum) on the advanced core with the not-taken predictor.
Every memory is the same flip-flop memory of 65,536 words (256 KiB). The
caches are small, so their contents stay readable in the visualizer:

- each L1 has 4 sets of one way and one-word lines;
- the L2 has 4 sets of two ways.

The arbiter and the cache controllers are gate-level and can be opened in
the explorer. The scenarios show structure, not speed: with a zero-wait
memory the caches cannot save much.

Every scenario is checked against the instruction-set reference at every
retired instruction, and its final registers and memory are checked too.

## A. Separate memories

`IMEM_` connects to `INSTRUCTION_MEMORY` and `DMEM_` to `DATA_MEMORY`, two
endpoints with no part in between. Fetches and data accesses never wait for
each other, but each memory holds a whole copy of the program image. This is
the advanced core's ordinary program scenario.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32i-five-stage-program21" title="Arrangement A: separate memories" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32i-five-stage-program21" class="md-button">Open arrangement A</a>

## B. One shared memory

Inside `MEMORY`, both ports reach `ARBITER`: `IMEM_` its `I_` port and
`DMEM_` its `D_` port. The arbiter's `MEM_` port connects to `RAM`. One
request at a time passes the arbiter. When both ports
ask in the same cycle, the side that did not go last goes first. A response
returns to the port that owns the request in flight, so the other port waits
while it is served.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32i-memory-b" title="Arrangement B: one shared memory" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32i-memory-b" class="md-button">Open arrangement B</a>

## C-ID. First-level caches

Inside `MEMORY`, `IMEM_` reaches `INSTRUCTION_CACHE` and `DMEM_`
`DATA_CACHE`. The caches' `MEM_` ports connect to the arbiter's `I_` and `D_`
ports, and the arbiter to `RAM`. A hit is answered by its cache and never reaches
the arbiter; only misses and the data cache's writes to memory compete for
it. The instruction cache is read-only.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32i-shared-memory" title="Arrangement C-ID: first-level caches" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32i-shared-memory" class="md-button">Open arrangement C-ID</a>

## D-0. A shared second-level cache

The ports reach the arbiter as in B. The arbiter's `MEM_` port connects to
`SHARED_CACHE`, and the cache's `MEM_` port to `RAM`. Every
request passes the arbiter first; the shared cache then answers fetches and
data accesses alike, so the two sides share its lines.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32i-memory-d0" title="Arrangement D-0: a shared second-level cache" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32i-memory-d0" class="md-button">Open arrangement D-0</a>

## D-ID. Both levels

The two L1 caches of C-ID sit above the arbiter and the shared cache of D-0
below it. An L1 miss crosses the arbiter and may hit in the L2 without
reaching memory.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32i-memory-did" title="Arrangement D-ID: both cache levels" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32i-memory-did" class="md-button">Open arrangement D-ID</a>

## Where code and data sit

In A each memory holds the whole program image. In the shared arrangements
the one memory holds it once. Every block keeps its own address, so code and
data share one address space at the bottom of memory. The programs of
[§ 4.8](experiments.md#memory-systems) use this map:

| Bytes | Contents |
| --- | --- |
| from `0x0000` | Program code (all four programs fit well below `0x1000`) |
| `0x1000` | Input data or pointer-chase nodes (up to 4 KiB) |
| `0x2000` | Output data |
| `0x3000` | Result word |
| below `0x4000` | Stack, growing down (`calls`) |

A program whose code and data images would put different bytes at one
address is refused in the shared arrangements rather than run as a different
program.

## Seeing both sides of one memory

The [memory inspector](../visualizer.md) shows the shared memory of every
shared arrangement as two panels: instructions on the left (disassembled) and
data on the right, each at the latest address its side used. In B and C-ID
it tells the sides apart by the arbiter's grant when the memory takes a
request. In D-0 and D-ID the shared cache sits in between; it serves one
request at a time, so each memory access it makes belongs to the request the
arbiter has in flight, whose side the arbiter's `OWNER_D` bit names. A line
the shared cache brought in for one side can later serve the other without
a memory access.

## Tests

- `RV32ISharedMemoryProgramTest` (C-ID), `RV32IMemoryArrangementBTest`,
  `RV32IMemoryArrangementD0Test` and `RV32IMemoryArrangementDIDTest` run
  program 21 through the arrangement against the instruction-set reference. A is `RV32IFiveStageCoreProgramTest/program-21`.
- `RV32ITopologyTest` runs programs through every arrangement over DRAM in
  the experiment tool, including write-back caches, and checks that an
  overlapping code and data image is refused.
- `MemoryInspectorIntegrationTest` checks the two-panel view natively and
  through the Python server.
