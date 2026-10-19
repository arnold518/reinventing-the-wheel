# § 3.6. Memory Timing

How long a memory access takes, where that time comes from, and how the
controller turns it into clock cycles. The general rules for delays and
profiles are in [§ 1 Time and Delay](../timing.md); this page applies them to
memories.

## Where an access spends its time

An access through an [endpoint](controllers.md) has three parts:

1. **Controller launch.** At the clock edge that accepts a request, the
   controller's registers drive the device's address, data and controls. This
   takes a flip-flop's clock-to-output delay and a few gates.
2. **Device access.** The device's own path, which differs by technology
   (below).
3. **Capture.** The controller takes the result at a later clock edge. The
   data must have arrived a flip-flop setup time, plus a small margin, before
   that edge.

The controller then holds the response in its registers until the requester
takes it.

### Asynchronous SRAM and ROM

These devices have no clock. A timing chain inside the device, built from
gates and delay lines ([3.2.6](building-blocks.md#memorytimer)), watches
the address and controls. Any change restarts it:

1. **Settle**: an address-change detector waits until the inputs have been
   stable for a short settle interval, then starts the access. The inputs the
   array sees are held copies, so a change in progress never reaches it.
2. **Precharge** (SRAM only): the bitlines are precharged; a write starts
   driving its data onto them when precharge ends.
3. **Decode**: a replica chain of gates as deep as the row decoder; then the
   selected row opens.
4. **Bitline**: the selected SRAM cell pulls one bitline of each pair down,
   or the ROM read column senses its row. The interval grows with the number
   of rows, because a longer bitline is slower.
5. **Sense and output**: the sense amplifier and the output stage; `READY`
   rises when the data on `READ_DATA` is valid.

The controller does not watch `READY` from cycle to cycle. It counts a fixed
number of **wait cycles**, chosen from the proven access time and the clock
period, exactly as a real system sets its memory wait states from its clock
frequency:

`access_cycles = ceil((controller launch + device access + setup + margin) / period)`

At the capturing edge it checks `READY` and reports status `Device` if the
device was not done.

### Clocked SRAM macro

An on-chip SRAM is usually a clocked macro: it registers the address, data
and controls at a clock edge and times its array internally from there. The
controller shows the request to the macro during the accepting cycle, the
macro registers it at that edge, and the wait cycles count from the edge to
the capture. With no address-change detection and no controller launch in its
path, it is the fastest memory after flip-flop storage.

### DRAM

A DRAM access is a sequence of commands on the clock: ACTIVATE opens a row
into the row buffer, READ or WRITE accesses a column of the open row, and
PRECHARGE closes it. Each command occupies the device for an interval:

| Interval | Datasheet name | `embedded` (SDRAM, Micron MT48LC16M16A2) | `desktop` (DDR4, Micron MT40A1G8) |
| --- | --- | --- | --- |
| Row open to column access | tRCD | 18 ns | 13.75 ns |
| Column read to data | CL (tAA) | 18 ns | 13.75 ns |
| Precharge to next ACTIVATE | tRP | 18 ns | 13.75 ns |
| Refresh interval | tREFI | 7.8 µs | 7.8 µs |
| Refresh cycle | tRFC | 60 ns | 350 ns |
| Retention | tREF | 64 ms | 64 ms |

Inside the device these intervals are delay lines: the wordline rising, the
sense amplifiers developing and restoring the row, the column read, the
precharge and equalize steps. A real SDRAM controller counts its intervals
in clock cycles; so does this one: each command waits
`ceil(interval / period)` cycles, and the device's own timing chains confirm
at that edge that the command has finished. So the cost of an access depends
on the row state:

- **Row hit** (the row is already open): READ only.
- **Row closed**: ACTIVATE, then READ.
- **Row conflict** (another row is open): PRECHARGE, ACTIVATE, READ.

The controller keeps the row open after an access (an open-page policy). It
counts cycles between refreshes and, when one is due, closes the open row and
issues REFRESH while new requests wait; the interval is tREFI divided by the
clock period, at least 24 cycles. A row not refreshed within its retention
time reads as X.

### NOR flash

NOR flash reads a word in a fixed access time (tACC 120 ns for the Infineon
S29GL256S in both realistic profiles), programs one word in 150 µs, and
erases a 128 KiB block in 410 ms. The device reports `BUSY` while it works: it
samples the end of its own timing chain at the shared clock and releases
`BUSY` one edge later, and the controller then takes the result, as a real
controller polls the chip's status.
Programming must be preceded by WRITE ENABLE, which the controller sends.

### Flip-flop storage

Reads settle through the read multiplexer within the cycle and writes happen
at the clock edge, so the response is ready in the same cycle. The
fixed-cycle cores rely on this.

## Measured latency

Every row below is simulated: one access at a time through the compact
endpoint of a 256 KiB memory, the size the RV32I experiments use, at the
advanced core's certified clock. The compact endpoint takes the same cycles
as the gate-level controller and device; `MemoryLatencyGatesTest` compares
them in every profile on a memory small enough to build from gates. The numbers are generated
by `documentation_timing_data` and checked by the test suite.

<!-- latency-table: all -->

These latencies include one cost a real controller does not have: the
response is registered, so the requester sees it one edge after the data is
captured, and a self-timed DRAM command is confirmed through one sampling
flip-flop. Against the datasheets, the DRAM accesses take about a quarter to
a third more cycles than a counted real controller would; the NOR read is
within a tenth.

## Large memories

A gate-level memory is limited to 4,096 cells, so a 64 KiB memory has no
gates to prove its delays on. Every device and endpoint is therefore also
proven at 64 and 128 words, and a larger memory gets a table in which each
delay keeps growing per doubling as it did between those two sizes. Such a
table is an estimate, and its evidence line says so. The wait cycles of a
large SRAM or ROM come from that estimate. Large SRAM arrays are built from
32-row subarrays joined by global wires, whose delay grows with their length.

## Where timing is checked

| Check | What it shows |
| --- | --- |
| `DelayRuleTest` | Every compact memory part is never faster than its gates, and close to them; self-timed parts take the same cycles at the clock |
| `MemoryDeviceTimingTest`, `MemoryEndpointTimingTest` | Device and endpoint timing against their contracts |
| `MemoryLatencyGatesTest` | The latency table's accesses take the same cycles through the gate-level endpoints (`unit`) |
| `DocumentationTimingDataTest` | The numbers on this site are measured again and must match |
| `SRAMCellTimingTest`, `DFlipFlopTimingTest` | The measured cell and flip-flop contracts of each profile |
