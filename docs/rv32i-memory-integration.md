# RV32I memory integration

All three cores expose the same universal REQ/RSP fields. The strict single-cycle
core and baseline require zero-wait timing; the transaction-capable pipeline also
supports delayed endpoints. No external RV32I adapter is required.

| User | Memory boundary | Default storage | Completion |
| --- | --- | --- | --- |
| Single-cycle core | Universal REQ/RSP | Two zero-wait `MemoryEndpoint<ff,32>` devices | One enabled instruction per rising edge after settling |
| Baseline pipeline | Universal REQ/RSP | Two zero-wait FF endpoints | No memory wait cycles |
| Advanced five-stage pipeline (`RV32IFiveStageCore`) | Universal REQ/RSP | Two fused FF endpoints by default; configurable technology | Same-edge completion or delayed responses |

Each default memory has 65,536 words (256 KiB). Capacity does not force extra CPU
cycles. Logical capacity and resident allocation are separate; native devices
share paged storage and explicit allocation/history budgets.

## Memory compatibility by core

Sharing the REQ/RSP interface does not mean every core handles every response
latency. The strict single-cycle and baseline cores assume acceptance and a
response in the request cycle. The advanced pipeline additionally tracks
outstanding requests and holds progress until the response arrives.

| Endpoint | Single-cycle | Baseline pipeline | Advanced pipeline | Suitable role |
| --- | --- | --- | --- | --- |
| `MemoryEndpoint<ff,32>` | Yes; default | Yes; default | Yes; default | Instructions and read/write data; zero-wait |
| `MemoryEndpoint<sram,32>` | No | No | Yes | Instructions and read/write data; current controller adds wait cycles |
| `MemoryEndpoint<dram,32>` | No | No | Yes | Instructions and read/write data; command timing and refresh |
| `MemoryEndpoint<rom,32>` | No | No | Yes, read-only | Provisioned instructions or constant data; stores fault |
| `MemoryEndpoint<nor-flash,32>` | No | No | Yes, reads | Provisioned instructions or constant data; ordinary stores cannot program flash |

These entries apply to the current controllers, not a claim that SRAM or ROM
must inherently take multiple CPU cycles. Endpoint fidelity does not change
compatibility: a fused `BehavioralMemoryEndpoint` and the corresponding visible
controller/device assembly keep the same contract. An endpoint may contain a
scalable `MemoryContainer<technology,32>` or a small expanded native device;
the container's native pins alone are not a drop-in replacement for REQ/RSP.
For FF storage, the small expanded device is `FFMemory<32>`.

The fixed-cycle program roots and `RV32ISingleCycleSystem` reject non-FF
configurations during construction. The cores themselves do not use
`REQ_READY=0` or `RSP_VALID=0` to wait. Manually wiring a delayed endpoint around
those guards can make them capture stale instructions/load data or advance past
unaccepted requests/stores. There is no automatic timing-contract trap.
The baseline's dependency and branch stalls do not supply memory-wait control.

A longer clock period allows combinational paths to settle; it does not remove
protocol phases that need additional edges. Use the advanced pipeline for
delayed endpoints. Supporting those endpoints in a fixed-cycle core would
require new CPU-side hold/completion control, not merely a different controller.

## Components and responsibilities

| Component | Responsibility |
| --- | --- |
| `MemoryContainer` | Scalable native FF/SRAM/DRAM/ROM/NOR behavior, with that technology's pins |
| `MemoryController` | Map transactions to native pins; FF completes without wait cycles, other technologies sequence operations and hold responses; owns no contents |
| `MemoryEndpoint` | Controller plus native device behind the common transaction interface |
| `BehavioralMemoryEndpoint` | Fused behavioral implementation of that assembly |
| `RV32IMemoryLanes` | Byte/halfword selection, store shifts/masks, load sign extension |
| `RV32IMemoryRequest` | Pipeline-side outstanding-request tracking and redirect draining |

The legacy `memory.scalable.*` catalog IDs retain their transaction contracts.
New native containers use `memory.device.*`; their schemas are not silently
interchangeable with the legacy IDs.

## Single-cycle path

Fetch owns only PC state, its incrementer, and direct read wiring. Memory is
combinational: address/lane formatting, alignment checks, and commit-gated WE.
Neither stage has a requester, payload register, response buffer, or memory array.

Stage-local WORD_ADDR/READ_EN/READ_DATA/FAULT and masked-write signals are
formatted by combinational port groups. The core boundary exposes IMEM_/DMEM_
REQ_VALID/READY, REQ_OP/ADDR/WRITE_DATA/WRITE_MASK and RSP_VALID/READY/READ_DATA/STATUS.
REQ_ADDR is an aligned byte address.

PC/register/memory writes capture old-state-derived values on the same rising
edge. DMEM_READ_EN also checks store addresses before authorizing WE. Faults
suppress normal writes. DONE is constant high, not a settling detector: the
clock period must accommodate the complete combinational path.

The system shares CLK and interface RST with its FF endpoints; controllers tie
native storage reset low. The root contains CLOCK, CORE and two endpoints.
Visible FF controller/device assemblies are supported too. Fixed-cycle factories
reject non-FF technologies, rather than silently using delayed memory.

## Advanced pipeline path

`RV32IFiveStageCore` retains IMEM_REQ/RSP and DMEM_REQ/RSP, byte-addressed aligned
transactions, held valid/payload, backpressure, and one outstanding operation
per port. Redirects drain obsolete instruction responses, including ABA changes.
A store is applied by the endpoint before its completion response; interface
reset cannot undo an already launched physical operation.

`rv32i::MemoryConfiguration` independently selects instruction/data technology,
capacity, and endpoint form. ROM rejects ordinary stores; flash requires
PROGRAM/ERASE rather than SW. DRAM sequencing and refresh belong to its
controller. Native DRAM still retains destructive reads, row state, and retention.

`expanded_endpoint=false` selects a fused behavioral endpoint.
`expanded_endpoint=true` selects a visible controller/device assembly;
setting `device.expanded_storage=false` keeps that controller visible while
using scalable native storage. Controller register count depends on port width,
not the number of stored words. Full cell expansion retains its 4,096-cell guard.

## Provisioning and inspection

`MemoryStateView` exposes bounded snapshots and optional pre-simulation image
initialization. Reset never reopens provisioning. Runtime writes use native
pins or transactions and retain the selected technology's rules. Expanded
devices without a host image capability use construction images or pin writes.

The isolated instruction answer-sheet system owns host-side `MemoryStorage`;
it does not construct an unconnected hardware memory. The compact native FF
device uses private `FFMemoryBehavioral<W>`, tested against the structural
`FFMemory<W>` contract. Neither host storage nor this direct device adds a
transaction layer. The superseded `Memory64Kx32` and generic legacy container
have been removed (see [cleanup](rv32i-cleanup.md)).

## Current validation

All 22 single-cycle program cases enforce at most one clock per instruction.
Stage tests cover both fidelities; integration covers overlapping source/
destination operands and the no-reading-back rule. Native memory equivalence,
controller/scalable-device composition, and 256 MiB sparse devices are covered
by `NativeMemoryDeviceContractTest`. `RV32IMemoryMigrationTest` covers the
zero-wait fixed-cycle paths and configurable pipeline endpoints.

Published pin contracts, behavioral examples, diagrams, and embedded rows:
[3.1](../site-docs/memory/3.1/index.md),
[3.1.6](../site-docs/memory/3.1.6.md),
[3.3](../site-docs/memory/3.3.md), and
[4.3](../site-docs/rv32i/4.4/index.md).

## Zero-wait endpoint timing regression

The current FF default yields the following counts for the transaction-capable
pipeline. These are simulated architectural clocks, not host runtimes or calibrated
physical-chip timings. SRAM remains selectable; these counts do not claim that
SRAM itself became faster.

| Program | Cycles | Load-use stalls | Memory wait cycles |
| --- | ---: | ---: | ---: |
| 17 | 77 | 0 | 0 |
| 18 | 70 | 0 | 0 |
| 19 | 103 | 32 | 0 |
| 20 | 85 | 0 | 0 |
| 21 | 134 | 16 | 0 |
| 22 | 87 | 8 | 0 |

## Historical transaction-only migration evidence

The following records describe the earlier wait-state single-cycle datapath
architecture. They are preserved as history, not current timing
or test-count claims.

### Pipeline timing regression values

These measured values use the existing program-test clock and default memory timing; they are simulator regression values, not calibrated hardware results.

| Program | Previous relay cycles | Direct-port cycles | Data-memory stall cycles |
| --- | ---: | ---: | ---: |
| 17 | 807 | 588 | 0 |
| 18 | 730 | 532 | 0 |
| 19 | 805 | 604 | 224 |
| 20 | 730 | 532 | 0 |
| 21 | 1125 | 828 | 112 |
| 22 | 784 | 580 | 112 |

Load-use and data-port stalls remain zero for these fixtures; flush counts remain 1, 1, 1, 16, 16, and 8.

### Direct-port migration verification

- 62 focused native tests passed, including all 44 programs, both core contracts, endpoint contracts/timing, loaders, the external ELF smoke test, saved-layout checks, Python bindings, and documentation/schema checks.
- The serial run took 130.55 seconds and peaked at 343,740 KiB RSS (about 336 MiB). This measures the test process tree, not hardware throughput or the memory needed by every possible structural circuit.
- The expanded integration checks run both core fidelities with containers and expanded ROM/SRAM endpoints, repeatedly pause ENABLE, and load a stored byte while neighboring SRAM bytes remain unknown. The single-cycle behavioral reference checks only the requested load lanes.
- The final native/WASM comparisons passed for single-cycle and pipeline, structural and behavioral: topology, recorded timing/state, and memory inspectors agree.
- Chrome checks passed for all four core/fidelity combinations, including direct-memory root placement, terminal state, memory inspection, and no backend API requests. Core/requester floorplans are also checked for containment and overlap.
- The strict MkDocs build passed; all 22 program checkpoint exports were refreshed from the rebuilt WASM engine.
- The final documentation browser run passed for 40 instruction entries, all 22 program pages, and 650 embedded checkpoints, including responsive table widths and preservation of saved fidelity preferences.

The external self-check ELF still retires 19 instructions. The direct-port version takes 107 single-cycle-datapath clocks and 191 pipeline clocks, versus 173 and 260 with the previous relay. These are simulator-specific uncached transaction costs, not calibrated hardware timings.
