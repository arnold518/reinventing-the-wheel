# RV32I direct transaction-memory ports

The single-cycle datapath (with wait cycles) and five-stage pipeline expose the common memory protocol directly. Separate instruction and data memories default to `MemoryContainer<sram,32>` with 65,536 words each. Program, ELF, and benchmark harnesses use the same wiring.

```text
                       +-> instruction MemoryContainer or MemoryEndpoint
CPU IMEM_REQ/RSP ------+
CPU DMEM_REQ/RSP ------+-> data MemoryContainer or MemoryEndpoint
                       shared CLK / RST
```

There is no external RV32I memory adapter and no container relay controller inside the CPU. The top-level program circuit contains only CLOCK, CORE, INSTRUCTION_MEMORY, and DATA_MEMORY. A native SRAM/DRAM/etc. device still needs its technology controller; the complete `MemoryEndpoint` exposes the universal interface.

## Components and responsibilities

| Component | Responsibility | Structural | Behavioral |
| --- | --- | --- | --- |
| `RV32IMemoryRequest` inside each core | Hold one request stable, track acceptance, acknowledge or discard its response | Yes: payload FFs, three state FFs, gates | Matching state model inside behavioral cores; not a separately selectable helper variant |
| `RV32IMemoryLanes` inside each core | Little-endian byte/halfword lanes, masks, sign extension, alignment preservation | Yes: rewires, muxes, gates | Matching operations inside behavioral cores; not a separately selectable helper variant |
| `RV32ISingleCycleCore`, `RV32IFiveStageCore` | Request/response scheduling, datapath and retirement | Yes | Yes |
| `MemoryContainer<technology,32>` | Scalable storage and technology-specific transaction behavior | No full-size expansion | Yes, backed by representative device contracts |
| `MemoryEndpoint<technology,32>` | Controller plus a separate native memory device, with the same external port | Yes | Yes |

The requester does not store memory contents, execute technology commands, or buffer responses. The endpoint owns a response until the CPU acknowledges it. Its only storage is the request payload, original address tag, and BUSY/SENT/ABANDONED bits.

## Public CPU ports

Both ports use the following signals, prefixed with `IMEM_` or `DMEM_`. Directions are relative to the CPU.

| Signals | Direction | Width | Meaning |
| --- | --- | --- | --- |
| `REQ_VALID`, `REQ_READY` | Output, input | 1 each | A request transfers on a rising edge with both high |
| `REQ_OP` | Output | 3 | Read = 0; ordinary write = 1 |
| `REQ_ADDR` | Output | 32 | Byte address of the accessed word |
| `REQ_WRITE_DATA` | Output | 32 | Store data shifted into the selected byte lanes |
| `REQ_WRITE_MASK` | Output | 4 | One write enable per byte lane |
| `RSP_VALID`, `RSP_READY` | Input, output | 1 each | A response transfers on a rising edge with both high |
| `RSP_READ_DATA` | Input | 32 | Endpoint-held read word |
| `RSP_STATUS` | Input | 3 | Zero = success; other status values become CPU access faults |
| `CLK`, `RST` | Input | 1 each | Shared core/endpoint signals, not separately forwarded per-port outputs |

The legacy public READ_EN, WRITE_EN, SIZE, SIGN_EXTEND, READY, FAULT, and CONSUME memory signals are removed. Internal pipeline stages can still describe a load/store in instruction terms; this is not a public memory-device interface.

## Request and completion behavior

1. When idle, the core's requester captures the next access at a rising edge.
2. It holds VALID and the payload stable until READY accepts the request. A fetch redirect cannot withdraw an already offered request.
3. Acceptance is not completion. The requester waits for the corresponding response.
4. A matching response becomes available to the datapath. The endpoint holds it during CPU stalls or ENABLE pauses.
5. The single-cycle datapath consumes it at an instruction attempt; pipeline stage advancement consumes it when that stage can proceed. Each transaction is consumed once.
6. A changed fetch tag makes the old request obsolete. ABANDONED remembers this even if the PC returns to the old address before the response arrives. The old response is drained without entering the pipeline; a new request is then captured.

Each port has one outstanding transaction; no bursts, IDs, speculation rollback, cache, or coherence are introduced. A capture/accept/response sequence takes multiple clocks. The current implementation also leaves a capture boundary between transactions rather than accepting another request on the response-consumption edge.

Byte and halfword loads select and extend the addressed lane; stores shift data and generate byte masks. Invalid halfword/word alignment, or reserved size 3, remains an invalid word address at the endpoint instead of silently rounding down to a legal access. The endpoint returns an error without changing storage. No CPU store is translated into flash program/erase.

Reset is common to the core and endpoint. It clears in-flight interface state; committed writes remain stored. There is no old relay that defers endpoint reset or replays requests. A native operation already launched behind a controller follows that endpoint's reset contract.

## Choosing storage

`rv32i::MemoryConfiguration` is accepted independently for instruction and data memory by `RV32IProgramRoot` and `RV32ISingleCycleSystem`:

```cpp
rv32i::MemoryConfiguration instruction, data;
instruction.technology = circuit::memory::Technology::ROM;
instruction.device.depth_words = 8;
instruction.device.image_hex = program_image_hex; // Little-endian image bytes.
data.technology = circuit::memory::Technology::SRAM;
data.device.depth_words = 4;

// The same wiring supports small expanded, native-device endpoints.
instruction.expanded_endpoint = true;
data.expanded_endpoint = true;
```

Leave `expanded_endpoint` false for scalable containers. Technology, capacity, timing, image, and host-resource budgets are separate configuration fields. Expanded endpoints retain their small-geometry limits; expanding a 256 KiB SRAM into cells is not the default. Normal component build profiles still control eligible endpoint/device abstractions.

The cores issue ordinary read and write operations. ROM rejects stores; NOR flash requires its separate program/erase operations and therefore rejects ordinary CPU stores. A DRAM endpoint/container retains its own refresh behavior. Exposing flash commands through CPU software would need an additional control-register/MMIO interface, not a silent change to `SW`.

## Program loading and inspection

Containers expose an optional `MemoryStateView` capability for inspecting storage and provisioning a program image **before the first evaluation**. ELF loading and program fixtures use that capability, not a concrete legacy-memory cast. Initialization/clearing is rejected once simulation starts; reset does not reopen this host loading window. Runtime software accesses still use pins and obey technology rules.

Fully expanded endpoints need not provide a host state/image capability. Initialize ROM through its construction image, and writable devices through their pins. The focused integration test runs a program from an expanded ROM endpoint against expanded SRAM without a host-memory shortcut.

`Memory64Kx32` remains available for independent legacy component tests and the isolated `RV32IReferenceSystem` answer-sheet oracle. It is no longer the memory in the executable program roots or ELF/benchmark systems. Keeping that oracle independent avoids checking the new transaction system against itself.

## Timing and validation

“Single-cycle” describes the datapath, not a promise of one clock per instruction with variable-latency memory. Both cores wait for actual responses. Removing the external relay reduces transaction overhead; it does not turn this uncached system into a one-instruction-per-cycle pipeline.

The focused integration test exercises the structural requester/formatter with all five memory technologies, each as a container and expanded endpoint. It checks lanes, signed loads, masks, alignment/range faults, ROM/flash restrictions, request and response backpressure, redirect/ABA handling, reset, and image provisioning. Both CPU systems also run a small ROM/SRAM program using either device form.

The existing 44 program cases compare architectural traces and memory writes between structural/behavioral cores and the instruction oracle. Core-level scenarios now drive the universal handshakes explicitly. Native/WASM parity checks cover topology, timestamps, state, and memory inspection; browser checks cover both fidelities and all 22 documentation programs.

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
