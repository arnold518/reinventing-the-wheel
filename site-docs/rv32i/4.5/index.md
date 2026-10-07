# § 4.5. Baseline Pipeline

`RV32IBaselinePipelineCore` overlaps independent instructions in five stages. Four clocked pipeline registers separate Fetch, Decode, Execute, Memory, and Writeback. Each instruction carries its own operands, destination, control, and fault status through these registers.

This is the simplest supported pipeline baseline: **stall-only dependencies, no forwarding, no branch prediction, and zero-wait FF endpoints**. It does not replace `RV32IFiveStageCore`, the existing forwarding/transaction-memory design.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| Four `RV32I…PipelineRegister` components | Instruction packets and validity between stages | Yes | Yes |
| `RV32IBaselineFetchStage` | Fetch PC, sequential/redirect selection, native instruction port | Yes | Yes |
| `RV32IBaselineDecodeStage` | Decoder and register file; independent writeback destination; no bypass | Yes | Yes |
| `RV32IBaselineExecuteStage` | Captured operands, shared ALU, branch/jump resolution | Yes | Yes |
| `RV32IBaselineMemoryStage` | Direct load/store lanes and qualified fault status | Yes | Yes |
| `RV32IWritebackStage` | Combinational result selection and retirement checks | Yes | Yes |
| `RV32IPipelineExecutionStatus` | Separate sticky halt/trap flags and cause | Yes | Yes |
| `RV32IBaselineDependencyUnit` | Read-after-write checks against Execute, Memory, and Writeback | Yes | Yes |
| `RV32IBaselineControl` | Pipeline hold/bubble enables, branch wait, precise stopping | Yes | Yes |
| `RV32IBaselinePipelineCore` | Complete zero-wait five-stage assembly | Yes | Yes |
| `RV32IBaselinePacketStatus` | Internal grouping of shared terminal/control-flow gates | Yes | No standalone variant |

<!-- diagram: baseline-pipeline -->

## Clock and state

During a cycle, each stage computes from its current input packet. The next rising edge captures all enabled stage outputs together. A value produced by a stage cannot pass through two pipeline registers on one edge.

The architectural register file is in Decode but receives its write address and value from Writeback. A consumer waits while a matching producer remains in EX, MEM, **or WB**. After the producer's writeback edge, the consumer reads the updated register file in the following cycle. No WB-to-Decode bypass is hidden in the model.

Memory stores occur at the end of MEM; register writes and retirement occur at the end of WB. The oldest instruction's halt/trap blocks younger side effects. `VALID=0` is a bubble: payload bits may remain populated, but they cannot write registers or memory or retire.

`CONTROL` is the movement coordinator, not the unit that computes branch results: `DEPENDENCIES` detects unavailable operands, Execute resolves branch/jump addresses, and Fetch/pipeline registers carry out the controller's enables and flushes. A stall does not stop the clock. See [control responsibilities](control.md#control-responsibilities) and the [worked data-hazard example](control.md#data-hazard-hold-the-consumer-advance-the-producer).

## Branches and jumps

When Decode recognizes a branch, JAL, or JALR, Fetch stops accepting new instructions. Once operands are available, the instruction enters Execute and the younger IF/ID slot becomes a bubble. Execute selects **either** the target **or** the fallthrough PC, then Fetch resumes from that address on the next cycle. Even an untaken conditional branch incurs this wait. There is no prediction table, speculative execution, or forwarding network.

An instruction may already be visible on the instruction-memory wires when the branch is recognized. It is not accepted into IF/ID and has no architectural effects, including when that unaccepted fetch reports a fault.

The [worked jump example](control.md#jump-stop-younger-fetches-let-the-jump-resolve) follows the jump from Fetch through retirement and shows exactly when the target instruction starts fetching. A branch/JALR that also needs an older result follows the [combined dependency and branch wait](control.md#a-branch-or-jalr-can-wait-for-data-too).

## Memory boundary

The baseline uses the same universal REQ/RSP pins as [the single-cycle core](../4.4/core.md), with a required zero-wait response. Program fixtures select separate FF endpoints for instructions and data: combinational reads and masked writes on the shared rising edge. The default representation is fused behavioral storage; the expanded endpoint exposes controller plus native device. Stage-local word-address pins are formatted by combinational port groups. The core contains no instruction/data storage or pending-request buffer.

Only the current FF endpoints meet this baseline's timing contract. The program root rejects non-FF configurations; bypassing that guard can cause incorrect execution, not automatic memory stalls. Register-dependency and branch stalls do not wait for `REQ_READY` or `RSP_VALID`. See the [memory compatibility table](core.md#memory-integration) for supported components and their storage options.

A clock cycle must allow its combinational paths to settle. The test period is a simulator timing allowance, **not** a measured physical clock frequency. Cache misses, variable latency, arbitration, forwarding, and prediction are outside this baseline. The existing advanced core remains available for transaction-memory experiments.

## Section map

| Section | Contract |
| --- | --- |
| [4.5.1. Pipeline Registers](registers.md) | Packets, encoded fields, capture/hold/bubble |
| [4.5.2. Fetch](fetch.md) | Native instruction reads and fetch PC |
| [4.5.3. Decode](decode.md) | Operand reads and separate WB writes |
| [4.5.4. Execute](execute.md) | ALU sharing and branch resolution |
| [4.5.5. Memory](memory.md) | Native data access and faults |
| [4.5.6. Writeback](writeback.md) | Result selection, retirement permission, precise status |
| [4.5.7. Dependencies and Control](control.md) | Stalls, branch waits, terminal priority |
| [4.5.8. Complete Core](core.md) | Assembly, public IO, program verification |

All behavioral equivalence claims in this section cover settled binary signals after reset. Compact models are not analog timing models and do not claim structural X/Z propagation equivalence. Structural subcomponents remain selectable for inspecting lower-level circuitry.
