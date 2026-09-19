# § 3.1. Memory Devices

This section defines the storage side of the memory architecture: cells, words or rows, banks, native devices, and the scalable `MemoryContainer`. Each technology keeps its own storage rules and native pins. A common transaction interface is introduced separately in [3.2](../3.2.md), and controllers connecting that interface to devices are documented in [3.3](../3.3.md).

Shared addressing and output circuitry is documented in [3.1.0. Helper Components](../3.1.0.md). The technology references below describe cells, banks, native access control, and complete devices.

## Device families

| Section | Storage organization | Device access | Distinguishing state |
| --- | --- | :---: | :---: |
| [3.1.1. Flip-flop storage](../3.1.1.md) | `FlipFlopCell` → `FFWord<W>` → `FFStorageBank<W>` → `FFMemory<W>` | Word address, edge-triggered masked writes | Clocked bits; native reset clears storage |
| [3.1.2. SRAM](../3.1.2.md) | `SRAMCell` → `SRAMRow` → `SRAMStorageBank<W>` → `SRAM<W>` | Asynchronous chip/write/output controls | Cross-coupled state, digital complementary bitlines |
| [3.1.3. DRAM](../3.1.3.md) | `DRAMCell` → `DRAMRow` → `DRAMStorageBank<W>` → `DRAM<W>` | ACTIVATE row, READ/WRITE column, PRECHARGE, REFRESH | Destructive sampling, row buffer, restoration, retention |
| [3.1.4. ROM](../3.1.4.md) | `ROMCell` → `ROMRow` → `ROMStorageBank<W>` → `ROM<W>` | Asynchronous read only | Immutable construction-time image |
| [3.1.5. NOR flash](../3.1.5.md) | `NORFlashCell` → `NORFlashRow` → `NORFlashStorageBank<W>` → `NORFlash<W>` | Clocked read/program/erase commands | Program 1→0; block erase to 1; no refresh |
| [3.1.6. Scalable memory container](../3.1.6.md) | `MemoryContainer<W,Model>` | Common request/response port | Compact controller-plus-device model for any of the five technologies |

`MemoryContainer` is not a sixth physical technology. It is a scalable representation of an entire memory endpoint. Its model determines whether the contents behave as FF storage, SRAM, DRAM, ROM, or NOR flash.

## Storage layers

```text
Native memory device
├── Address decoding and access/command control
├── Storage bank
│   ├── FF: words → clocked cells
│   └── SRAM/DRAM/ROM/NOR: rows → cells in physical bit columns
│       └── Technology-specific column sensing / row-buffer state
├── Read selection and, where writable, write distribution
└── Output publication / output enable
```

| Layer | Responsibility | What it does not provide |
| --- | --- | --- |
| Cell | Represent one bit and the technology's primitive state changes | Address decoding or a universal bus |
| Word / row | Group cells under shared local controls | A complete addressed memory interface |
| Bank | Organize words/rows and their column behavior | Request/response buffering |
| Native device | Add addressing, selection, access phases, and public device pins | A technology-independent transaction protocol |
| Memory endpoint | Combine access control with storage behind the common transaction port | A cache, interconnect, or processor memory policy |

These are common **roles**, not a common writable-cell interface. FF has a clocked write enable; SRAM uses complementary bitlines; DRAM requires destructive sampling and restoration; ROM has no write operation; NOR has separate program and erase operations. A row's width can exceed the externally addressed word width. Selecting one word does not mean that every technology performs DRAM-style ACTIVATE and column commands.

## Width, capacity, and geometry

W is the number of bits in one addressed word. N is the number of words. K is the number of words sharing one matrix row. W selects a C++ template specialization; N and K are construction-time parameters.

| Parameter | Meaning | Supported values |
| --- | --- | --- |
| W | Bits per externally addressed word | 8, 16, 32 for the public device families |
| `depth_words` (N) | Total addressable words | Positive; limits depend on device and implementation |
| `words_per_row` (K) | Words sharing a matrix wordline | Power of two dividing N; FF words do not use K |
| N / K | Number of rows | Derived |
| W × K | Number of bit columns per row | Derived |
| N × W / 8 | Capacity in bytes | Derived |

For word address A, `row = A / K`, `slot = A % K`; bit b occupies physical column `slot * W + b`. For example, W=8, N=4, K=2 describes two rows of 16 cells, with a capacity of four bytes. SRAM permits K up to 64; the native DRAM/ROM/NOR devices permit K up to 16. Their individual references define the full constraints.

Do not confuse **word address**, **row address**, and **byte address**. Native FF/SRAM/ROM use word addresses; native DRAM's command selects whether its address means row or column slot. The transaction port and memory inspector use byte addresses. A W-bit aligned transaction at byte address B accesses word `B / (W/8)`; the controller performs the native mapping.

Expanded devices and pin-expanded banks are limited to **4,096 cells**. Behavioral banks keep their many row/column pins and therefore keep the bank's size limits. A behavioral native device removes those internal pins; a `MemoryContainer` additionally models the transaction-facing controller. These are different abstraction boundaries, not interchangeable labels for the same interface. Native SRAM supports up to 2^24 words; native DRAM/ROM/NOR support up to 2^20 words. Container capacity instead follows its 32-bit byte-address and host-resource limits; see [3.1.6](../3.1.6.md#configuration).

## Device and transaction boundaries

```text
Requester / future cache
         │ common transactions (§3.2)
         ├── MemoryEndpoint (§3.3)
         │     ├── MemoryController ── native pins ── native memory device
         │     └── The controller owns access state; the device owns contents.
         └── MemoryContainer (§3.1.6)
               └── Compact controller + technology model + backing storage
```

The two branches are alternative implementations of a transaction-facing endpoint. A native device can be used directly through its native pins. A standalone `MemoryController` contains no storage and must be connected to a device. A `MemoryContainer` already includes controller behavior and can be connected directly to a transaction requester; it does not require a second technology controller.

Native FF reset clears FF storage. Transaction reset only resets the interface and drains already launched work; it is not connected to native FF storage reset and does not erase contents. Caches, arbitration, coherence, and accelerator access sit above this endpoint and are not implemented by these storage components.

The RV32I single-cycle and five-stage program systems use separate 32-bit SRAM-model `MemoryContainer` endpoints (256 KiB each by default). Each core directly exposes the common `IMEM_REQ_*` / `IMEM_RSP_*` and `DMEM_REQ_*` / `DMEM_RSP_*` transaction ports. Byte/halfword formatting and request tracking belong inside the CPU; no external CPU-memory adapter or relay controller is needed. A small controller/device `MemoryEndpoint` can replace a container without changing this wiring. Native SRAM or DRAM pins alone are not interchangeable with transaction pins.

`MemoryBusAdapter32`, `Memory64Kx32`, and `LegacyMemoryContainer` retain their older interfaces for existing standalone tests and the isolated instruction answer sheet. They are not used on the migrated CPU memory ports and are not aliases for the transaction-facing container.

## Structural and behavioral implementations

Structural implementations expose the cells, rows/words, banks, and peripheral logic. Where transistor or capacitor physics is outside this digital engine, charge storage and sensing are explicitly identified terminal digital primitives. “Structural” therefore describes composition, not a claim of transistor-level electrical simulation.

Behavioral implementations compress an existing lower-level design while preserving its external contract at settled checkpoints. Tests compare small structural and behavioral instances. Internal events and propagation traces need not be identical. A primitive is not given a redundant second implementation with the same code; the component tables state which implementations actually exist.

Four-state values are `0`, `1`, `X` (unknown), and `Z` (released output). Stored Z becomes X. DRAM models retention, refresh, and destructive reads digitally. ROM and flash retain their values within an instance without refresh; this does not imply persistence after destroying the simulator. Power, process variation, wear, analog bitline contention, and calibrated chip timing are outside these contracts. Simulator ticks are abstract time units.

## Inspection, layouts, and verification

Expanded banks place rows vertically and cells left to right in numeric column order. Column sensing aligns with those cells; control and decoding remain beside storage. These are logical diagrams, not semiconductor floorplans. Behavioral devices show a bounded memory window and touched words instead of thousands of cells.

The memory inspector reads a storage snapshot through `MemoryStateView`; it does not issue a memory operation. Command-device address pins can change meaning between phases, so they are not a persistent linear read cursor. DRAM idle expiry is evaluated on access/refresh: an old snapshot alone does not establish that charge has survived retention. The DRAM/ROM/NOR native devices and transaction container reject unrestricted debug loading. A container additionally provides explicit image provisioning before its first simulator evaluation, used by the RV32I program/ELF loaders. After simulation starts, writes must use transactions; reset does not reopen the image loader. Expanded devices without a host image hook must use their construction image or native/transaction operations.

Use `record_history=0` when content time travel is unnecessary. This disables memory-content history, not the simulator's separate wire history. Large containers additionally provide allocation and history budgets; native compact devices do not all share those resource controls.

Each component reference follows IO pins → behavior → C++ example → implementation → examples → source/tests. The complete scrollable example table includes checkpoint number and absolute settled time. “Not checked” means no assertion for that pin at that checkpoint, not a zero value. Fidelity changes can change checkpoint times.

`MemoryDeviceContractTest` and `MemoryDeviceTimingTest` cover native DRAM/ROM/NOR devices and shared helpers. FF and SRAM have their own contract tests. `MemoryEndpointContractTest` and `MemoryEndpointTimingTest` compare transaction endpoints with the scalable container, including masks, errors, reset, backpressure, and refresh. These are contract checks, not hardware-performance calibration.
