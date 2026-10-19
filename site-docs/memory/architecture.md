# § 3.1. Memory Architecture

This chapter builds memories the way the rest of the project builds logic:
from cells, through rows and banks, to complete devices, and then puts a
common transaction port in front of them. Five technologies share that
structure: flip-flop storage, SRAM, DRAM, ROM and NOR flash. Caches, which
sit above the transaction port, have their [own section](caches.md).

| Section | What it covers |
| --- | --- |
| 3.1 Memory Architecture (this page) | Layers, addressing, the two forms of every device, timing in memories, inspection |
| [3.2 Shared Building Blocks](building-blocks.md) | Decoders, column multiplexers, write distribution, read columns, output buffers, and the timing parts the devices and controllers are built from |
| [3.3 Memory Technologies](technologies/index.md) | The five technologies, each from cell to device, and the scalable native devices |
| [3.4 Transaction Protocol](transactions.md) | The request/response port every memory and cache uses |
| [3.5 Controllers and Endpoints](controllers.md) | The controller that turns transactions into native device operations, and the endpoint that pairs it with a device |
| [3.6 Memory Timing](timing.md) | How access times are built, and measured latencies in every timing profile |
| [3.7 Caches](caches.md) | Caches above the transaction port |

## Storage layers

<!-- diagram: memory-storage -->

| Layer | What it does | What it does not do |
| --- | --- | --- |
| Cell | Holds one bit and makes the technology's own state changes | Address decoding |
| Word or row | Groups cells under shared controls | A complete addressed interface |
| Bank | Organizes words or rows and their column circuits | Request or response buffering |
| Native device | Adds addressing, access control and the technology's public pins | A technology-independent protocol |
| Endpoint | Puts a controller in front of the device: the common transaction port | Caching or arbitration between requesters |

These are common **roles**, not a common cell interface. A flip-flop cell is
written on a clock edge; an SRAM cell through a pair of complementary
bitlines; a DRAM cell is read destructively and must be restored; a ROM cell
cannot be written at all; a NOR-flash cell is programmed from 1 to 0 and
erased back to 1 a block at a time. A row can be wider than one addressed
word, and selecting a word does not mean that every technology opens a row
the way DRAM does.

## Width, capacity and geometry

W is the number of bits in one addressed word, N the number of words, and K
the number of words that share one matrix row. W selects a C++ template
specialization; N and K are construction parameters.

| Parameter | Meaning | Supported values |
| --- | --- | --- |
| W | Bits per addressed word | 8, 16, 32 |
| `depth_words` (N) | Number of addressable words | Positive; the capacity must fit a 32-bit byte address |
| `words_per_row` (K) | Words sharing one wordline | A power of two dividing N: up to 16 in gate-level devices, up to 1,024 in compact ones (SRAM: 64); flip-flop storage has no rows |
| N / K | Number of rows | Derived |
| W × K | Bit columns per row | Derived |
| N × W / 8 | Capacity in bytes | Derived |

For word address A, `row = A / K` and `slot = A % K`; bit b of that word is
physical column `slot × W + b`. For example W = 8, N = 4, K = 2 is two rows of
16 cells, four bytes in all.

Three kinds of address appear in this chapter. Native flip-flop, SRAM, ROM
and NOR-flash devices take **word addresses**. A DRAM command says whether
its address is a **row** or a column slot. The transaction port, the caches
and the memory inspector use **byte addresses**: a W-bit aligned access at
byte address B reaches word `B / (W/8)`, and the controller does that
mapping.

A gate-level device or bank is limited to **4,096 cells**. Larger memories
use the compact (behavioral) form of the same device, which stores contents
in host pages and is limited only by the 32-bit byte address and the host's
memory budgets ([3.3.6](technologies/scalable.md)). The controller does not
grow with the memory: its registers depend on the port width, not the depth.

## Devices, controllers and endpoints

<!-- diagram: memory-endpoints -->

A **native device** has the pins its technology really has: chip and output
enables for SRAM and ROM, a command bus and a clock for DRAM and NOR flash, a
clock and write enable for flip-flop storage. A requester that speaks that
interface connects there directly.

A **controller** turns the common [transaction protocol](transactions.md)
into that native interface: it checks each request, sequences the device's
signals over the clock cycles the access needs, and holds the response until
the requester takes it. A **memory endpoint** is a controller and one device
of the same technology and geometry. The RV32I cores and the caches talk to
endpoints only.

The two device boxes in the figure are **alternatives**, not two memories:
the gate-level device or its compact form, with the same native pins. A
fused compact form of the whole endpoint also exists
(`BehavioralMemoryEndpoint`); it is the fast form used for large memories
and long programs, and it takes the same number of cycles as the gates.

Reset of the transaction port (`RST`) resets the interface, not the
contents: it drops pending handshakes and lets an operation already sent to
the device finish. Flip-flop storage has its own native reset, which the
controller holds low.

## Two forms of every device

Every memory part exists in a **structural** form, built from smaller parts
down to gates, and most have a **behavioral** form with the same pins that
computes the same results directly ([3.3](technologies/index.md) lists which
exist). The component tables on each page say which forms a part has; a part
is never given a second implementation that would only repeat the first.

Some electrical behavior cannot be built from logic gates: the charge on a
DRAM cell, a bitline being pulled down, a sense amplifier deciding. Each of
these is a small named **primitive** whose behavior and delay are stated on
its page, and "structural" means composition down to those primitives, not a
transistor-level simulation. Everything around them, the address decoders,
the timing chains that sequence an access, the controllers, is built from
gates.

The behavioral form must match the structural form at every recorded
checkpoint of its scenario, and it is timed by the delays proven on the
gates. Tests compare small instances of both forms; internal events and
glitches need not be identical.

## Time in memories

A memory access takes time for two reasons. Its logic (decoders,
multiplexers, the controller) takes gate delays, like any other circuit. Its
electrical steps (bitline discharge, sensing, DRAM restore, flash
programming) are delay lines whose lengths come from the active
[timing profile](../timing.md): in `unit` they are fixed tick counts, in
`embedded` and `desktop` the on-chip SRAM intervals are FO4 multiples compared
against the CACTI SRAM model and the external chips' intervals come from the
datasheets of real SDRAM, DDR4 and NOR-flash parts; they grow with the number
of rows where longer lines are slower.

The asynchronous devices (SRAM, ROM) **time themselves**: a chain of gates and
delay lines inside the device starts when the address or controls change and
raises `READY` when the data is valid. The clocked devices (flip-flop storage,
the on-chip SRAM macro, DRAM, NOR flash) take commands on clock edges. The
controller counts the cycles an access needs at the clock it runs on.

Each component's **Timing** section shows its proven delays in every
profile. [3.6 Memory Timing](timing.md) explains how an access time is built
and gives the measured latency of every technology.

## Unknown values

Values are four-state: `0`, `1`, `X` (unknown) and `Z` (released). A stored Z
becomes X. An unknown address or control makes the outputs unknown, and an
unknown value written to a cell is stored as X. DRAM models retention and
refresh: a row that has not been refreshed within its retention time reads
as X. ROM and flash keep their contents for the life of the simulation
without refresh. Wear, power and analog bitline contention are not modelled.

## Inspection and layouts

Gate-level banks are drawn with rows top to bottom and cells left to right
in column order, with sensing under the columns and decoding beside the
storage. These are logical drawings, not chip floorplans. Compact devices
show a window of memory words and the words recently touched instead of
thousands of cells.

The memory inspector in the [simulator](../visualizer.md) reads a snapshot
through `MemoryStateView` and never issues an operation. A compact device can
be given an initial image before its first evaluation; after that, contents
change only through its pins or through transactions. Set
`record_history=0` when time travel over memory contents is not needed; this
is separate from the simulator's wire history.

## How the component pages are organized

Each component on the following pages has the same sections:

1. **IO pins**: every pin, its direction and width.
2. **Behavior**: what the part does, a short C++ reference of its values,
   and how it treats unknown inputs.
3. **Implementation**: the structural form and, where it exists, the
   behavioral form.
4. **Timing**: the proven delays in every profile, generated from the delay
   tables the simulator uses.
5. **Examples**: every checkpoint of its scenario, recorded in the `unit`
   profile, an embedded simulator, and a link to open it full size. "Not
   checked" in a table means the scenario makes no assertion on that pin at
   that checkpoint, not that its value is zero.
6. **Source and tests**.

The C++ references describe settled binary values only. They are not
simulator implementations and do not model time.
