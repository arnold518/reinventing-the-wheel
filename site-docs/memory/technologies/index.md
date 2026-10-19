# § 3.3. Memory Technologies

Each technology is built the same way, from a one-bit cell through a row and
a bank to a complete native device, following the
[layers of 3.1](../architecture.md#storage-layers). What differs is how a
cell holds its bit, how it is read and written, and what timing the device
needs.

| Section | Storage chain | Device access | What sets it apart |
| --- | --- | --- | --- |
| [3.3.1 Flip-flop storage](flip-flop.md) | `FlipFlopCell` → `FFWord<W>` → `FFStorageBank<W>` → `FFMemory<W>` | Word address; writes on the rising clock edge with byte enables | Clocked bits; a native reset clears storage; reads are combinational |
| [3.3.2 SRAM](sram.md) | `SRAMCell` → `SRAMRow` → `SRAMStorageBank<W>` → `SRAM<W>` | Asynchronous chip, write and output enables, or a clocked on-chip macro | Cross-coupled cells on complementary bitlines; the device times its own access and raises `READY` |
| [3.3.3 DRAM](dram.md) | `DRAMCell` → `DRAMRow` → `DRAMStorageBank<W>` → `DRAM<W>` | Clocked commands: ACTIVATE a row, READ or WRITE a column, PRECHARGE, REFRESH, burst READ | Destructive reads, a row buffer, restore, retention and refresh |
| [3.3.4 ROM](rom.md) | `ROMCell` → `ROMRow` → `ROMStorageBank<W>` → `ROM<W>` | Asynchronous read with chip and output enables | Contents fixed at construction; the device raises `READY` when data is valid |
| [3.3.5 NOR flash](nor-flash.md) | `NORFlashCell` → `NORFlashRow` → `NORFlashStorageBank<W>` → `NORFlash<W>` | Clocked commands: READ, WRITE ENABLE, PROGRAM, ERASE BLOCK | Programming clears bits (1 → 0); erase restores a whole block to 1; long program and erase times |
| [3.3.6 Scalable native devices](scalable.md) | The compact form of each device above | Same native pins | Large capacity in host pages, no cell limit |

## How the technologies differ in time

| Technology | What decides when an access is done |
| --- | --- |
| Flip-flop | Gates only: reads settle through the read multiplexer; writes take effect at the clock edge |
| SRAM, asynchronous chip | The device's own timing chain (address-change detection, decode, bitline, sense) raises `READY` |
| SRAM, clocked macro | The macro registers the request at a clock edge and times the array internally |
| ROM | Like the asynchronous SRAM, read only |
| DRAM | Datasheet intervals (tRCD, CL, tRP) counted by the controller in clock cycles; the device checks them with its own timing chains |
| NOR flash | Datasheet intervals; the device reports `BUSY` until a read, program or erase is done |

[3.6 Memory Timing](../timing.md) gives the measured latency of every access
in every profile, at the certified clock.
