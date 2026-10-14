# § 4.6.5. Memory Wait Handling

The advanced core accepts the same universal memory interface as the other cores, but it also handles delayed acceptance and delayed responses. The default remains separate zero-wait FF instruction and data endpoints; no cache is introduced here.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IMemoryRequest` | CPU-side ownership of one outstanding request and its response | Yes | Yes; the advanced core builds its two requesters structurally |
| `RV32IMemoryLanes` | Load extension, byte-lane placement and masks | Yes | Yes; the cores build theirs structurally |
| `RV32IPipelineCoordinator` | Combine completion, hazards, recovery and precise stopping | Yes | Yes |

## Responsibilities

An endpoint/controller knows how to operate its memory device. A CPU requester remembers which instruction owns the operation and whether the CPU has consumed its response. These are different responsibilities. The native device stores bytes; the pipeline does not duplicate that storage.

The full requester pin contract and binary C++ state machine remain in [4.3.5. Memory Access](../4.3/memory-access.md). The advanced core instantiates `FETCH_REQUEST` and `DATA_REQUEST`, one per port. A zero-wait request and response can complete on the same edge through the bypass. Once captured, a request's payload stays stable until accepted; acceptance is not the same event as completion. Each accepted store occurs exactly once.

The core's `FETCH` child is `RV32IFetchStage`. In this assembly, `external_next_pc=1` makes `REDIRECT_PC` the PC register's next value from prediction; `PC_REDIRECT` remains on the shared stage interface but does not select a local PC+4 path. `FETCH_VALID` qualifies the outgoing IF/ID packet while `FETCH_PC` and `IMEM_ADDR` expose the stored byte PC. The `MEMORY` child is `RV32IMemoryStage`: it consumes the EX/MEM packet and `DMEM_READ_DATA`/`DMEM_FAULT`, emits the MEM/WB packet and stage-local memory fields, and exposes `LOAD_REQUEST`, `STORE_REQUEST`, faults, and forwarding signals to the coordinator and later stages. Neither child holds a pending memory transaction; the requesters own that state.

## Pipeline behavior

| Condition | Fetch / IF/ID | ID/EX, EX/MEM, MEM/WB | Register write / training |
| --- | --- | --- | --- |
| No wait or hazard | Accept one instruction | Advance normally | Qualified WB write; eligible branch trains |
| Instruction response absent | PC holds; IF/ID becomes invalid after its old instruction moves | Older instructions continue | Older work may retire |
| Data response absent for a MEM operation | Hold | Hold all packets | No repeated WB commit or predictor training |
| Load-use hazard | Hold consumer | Bubble ID/EX; advance older packets | Normal older work continues |
| Prediction correction | Correct PC; invalidate younger packet | Bubble ID/EX; resolved instruction advances | Train resolved branch once |
| Older data fault | Discard younger work | Kill EX result; carry fault to WB | Younger branch does not train |
| Terminal instruction retires | Stop accepting work | Clear remaining valid slots | Record halt/trap in separate status component |

`VALID=0` suppresses side effects even if payload bits remain. Stores are authorized only for the oldest eligible MEM operation. On a redirect, an already issued instruction request cannot simply disappear: its requester drains an obsolete response and starts the current fetch when permitted. Returning later to the same address does not revive an abandoned request.

## Coordinator IO pins

All pins below are one bit. The coordinator is combinational; CLK/RST belong to the stateful core components. During reset the core gates its effective ENABLE.

| Pins | Direction | Description |
| --- | --- | --- |
| `ENABLE`, `HALTED`, `TRAPPED` | Input | Whether normal execution is permitted |
| `IMEM_READY`, `DMEM_READY` | Input | Requester DONE: complete response for this instruction, **not raw REQ_READY** |
| `LOAD_USE_HAZARD` | Input | Immediate load dependency |
| `TERMINAL_PENDING`, `EX_PRETERMINAL`, `MEMORY_TERMINAL`, `WB_TERMINAL` | Input | Terminal work at different points; stop younger fetches |
| `EX_REDIRECT` | Input | Saved/actual next-PC mismatch from recovery |
| `MEMORY_FAULT`, `RETIRE_TERMINAL` | Input | Older data fault; terminal WB is retiring |
| `MEM_LOAD`, `MEM_STORE` | Input | Qualified MEM operation intents |
| `ACTIVE`, `IMEM_READ_ENABLE`, `DMEM_READ_ENABLE`, `DMEM_WRITE_ENABLE` | Output | Execution and request permissions |
| `FETCH_RESPONSE_TAKE` | Output | Consume an admitted instruction response, or drain one after a flush, halt or trap |
| `DATA_REQUEST_ISSUE` | Output | Present a data request when either data-read or data-write permission is asserted |
| `FETCH_VALID`, `FETCH_PC_WRITE`, `FETCH_PC_REDIRECT` | Output | Accept fetched instruction, advance/correct PC |
| `IF_ID_WRITE`, `ID_EX_WRITE`, `EX_MEM_WRITE`, `MEM_WB_WRITE` | Output | Capture or hold each packet register |
| `IF_ID_FLUSH`, `ID_EX_FLUSH`, `EX_MEM_FLUSH`, `MEM_WB_FLUSH` | Output | Write a bubble when the corresponding WRITE is enabled |
| `COMMIT_ENABLE` | Output | Permit combinational Writeback's edge-qualified commit |
| `PIPELINE_STALL`, `LOAD_USE_STALL`, `MEMORY_STALL`, `DATA_PORT_STALL`, `PIPELINE_FLUSH` | Output | Combinational observations; no counter storage |

MEM_LOAD and MEM_STORE cannot both be true for a legal core packet. The component still defines that input combination: serialize the conflict, let WB clear and suppress a simultaneous read. `DATA_PORT_STALL` is therefore normally zero in this core.

Response consumption and data issue are CPU policy, computed inside the coordinator:

```text
FETCH_RESPONSE_TAKE = FETCH_VALID OR ((HALTED OR TRAPPED) OR IF_ID_FLUSH)
DATA_REQUEST_ISSUE  = DMEM_READ_ENABLE OR DMEM_WRITE_ENABLE
```

Pausing alone does not discard a fetched response. Halt/trap still permits draining while paused, without admitting the response into IF/ID. These outputs grant permission; the requester still checks whether it owns a matching response and retains its handshake state.

The advanced core connects the instruction requester to `ACTIVE`, not `IMEM_READ_ENABLE`. This permits a speculative fetch while a terminal instruction is pending, removing the late EX/MEM terminal-to-instruction-data-to-prediction path. `FETCH_VALID` still prevents admitting such an instruction. IMEM must be side-effect-free instruction storage, not a read-sensitive MMIO peripheral. The existing request holder preserves stalled payloads and discards stale responses after a PC change; registered halt/trap also drains a pending response without accepting it into IF/ID. No additional response buffer is introduced. `IMEM_READ_ENABLE` remains the coordinator's conservative non-speculative permission for standalone users.

### C++ example

The output fields follow the IO table. This includes every coordinator output, including its defensive simultaneous-load/store behavior.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:coordinator"
```

## Implementation

### Structural implementation

The coordinator is a grouped gate network producing enables, flushes, fetch-response consumption and data-request issue; it owns no instruction registers. Its `FETCH_STOPPED` → `FETCH_DISCARD` → `FETCH_CONSUME` OR chain implements the consumption rule, and `DATA_ISSUE` combines the two data permissions. The requesters independently retain only their outstanding transaction state. Memory-stage data calculations, fault qualification and forwarding-value selection remain combinational.

### Behavioral implementation

The coordinator evaluates the same equations. The behavioral core uses the same one-outstanding requester model and packet hold/advance rules; changing fidelity does not replace a delayed endpoint with an instant one.

## Suitable memories

| Memory endpoint | Advanced pipeline | Single-cycle / baseline |
| --- | --- | --- |
| `MemoryEndpoint<ff,32>` | Supported; default, zero-wait | Supported; required current timing model |
| `MemoryEndpoint<sram,32>`, `MemoryEndpoint<dram,32>` | Supported with stalls | Not supported as drop-in delayed memories |
| `MemoryEndpoint<rom,32>` | Suitable for instruction/read-only data; writes fault | Not supported with its delayed controller |
| `MemoryEndpoint<nor-flash,32>` | Protocol-compatible reads; normal stores fail because flash programming is a different operation | Not supported |

Each endpoint may expose controller plus structural device, controller plus scalable native `MemoryContainer`, or a fused behavioral endpoint with the same transaction contract. The core sees the endpoint, not a bare container. Technology timing and supported commands remain the controller's contract in [3.3](../../memory/3.3.md). A huge behavioral device changes capacity and simulation cost, not the bus pins or CPU wait logic.

## Examples and tests

<!-- simulator-examples: rv32i-pipeline-coordinator RV32IPipelineCoordinatorTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-pipeline-coordinator&amp;embed=1" title="Advanced pipeline movement coordinator" loading="lazy" allowfullscreen></iframe></div>

`RV32IPipelineCoordinatorTest` compares the gate and behavioral equations. `RV32IMemoryMigrationTest` checks request acceptance/backpressure, response holding, redirected/obsolete requests, pause/reset, masked stores and delayed ROM/SRAM programs, including every predictor policy. `RV32IAdvancedExamplesTest` validates the displayed coordinator code against all registered rows.
