# § 4.4. Single-Cycle Core

`RV32ISingleCycleCore` connects six logical datapath stages and a shared execution-status unit. Each stage has structural and behavioral implementations under the same pin contract. [4.1](../4.1/index.md) specifies the instructions; [4.2](../4.3/index.md) specifies the reused building blocks.

Each enabled instruction completes at one rising edge after the full combinational path settles. Instruction and data memories read asynchronously; stores, PC, and register writes commit together. There are no request buffers, IF/ID/EX/MEM/WB pipeline registers, or overlapped instructions. Variable-latency endpoints are supported by the advanced `RV32IFiveStageCore`, not this strict single-cycle core or the baseline pipeline. See the [memory compatibility table](core.md#memory-integration).

## Components

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleFetchStage` | PC storage, PC+4, direct instruction read, fetch status | Yes | Yes |
| `RV32ISingleCycleDecodeStage` | Instruction decoding and the architectural register file | Yes | Yes |
| `RV32ISingleCycleExecuteStage` | Shared ALU, operand selection, branch comparison, jump/branch targets | Yes | Yes |
| `RV32ISingleCycleMemoryStage` | Direct data access, commit-gated stores, and lane formatting | Yes | Yes |
| `RV32ISingleCycleWritebackStage` | Select the value to write to a destination register | Yes | Yes |
| `RV32ISingleCycleNextPCStage` | Select the next PC and check an active transfer's alignment | Yes | Yes |
| `RV32ISingleCycleCore` | Connect the six stages, execution status, and two external memory ports | Yes | Yes |

Columns describe registered simulator implementations, not the simplified C++ examples. `RV32IExecutionControlStatusUnit` is reused from [4.3.6](../4.3/execution-status.md), with both implementations; it is not a seventh datapath stage.

## Contents

| Section | Coverage |
| --- | --- |
| [4.4.1. Fetch](fetch.md) | PC, PC+4, direct instruction read, reset |
| [4.4.2. Decode and Register Read](decode.md) | Decoding controls, two reads, one completion-edge write, x0 |
| [4.4.3. Execute](execute.md) | Operand selectors, one shared ALU, branch conditions, target generation |
| [4.4.4. Memory Access](memory.md) | Direct pins, byte lanes, sign extension, faults, store permission |
| [4.4.5. Writeback](writeback.md) | Zero/ALU/load/link selection; no storage |
| [4.4.6. Next PC](next-pc.md) | Fallthrough versus transfer, target alignment; no PC storage |
| [4.4.7. Complete Core](core.md) | Full wiring, completion, compatible memory endpoints, instruction examples and tests |

Each stage page follows **IO pins → behavior and C++ example → structural/behavioral implementation → examples and embedded simulation → tests**. Its scrollable example table contains all recorded rows, including checkpoint numbers and simulator times. The C++ examples reuse explicitly linked 4.3 models rather than redefining the decoder, ALU, or lane operations.

## Datapath and state ownership

### Stage roles

<!-- diagram: single-cycle-stages -->

Fetch supplies IMEM_WORD_ADDR/READ_EN and receives IMEM_READ_DATA/FAULT. Memory supplies DMEM_WORD_ADDR/WRITE_DATA/WRITE_MASK/READ_EN/WE and receives DMEM_READ_DATA/FAULT. Combinational port groups translate these stage-local pins into universal byte-addressed REQ/RSP fields. The system connects two zero-wait FF endpoints on the CPU clock. Next PC is a separate selection stage, shown below.

| Value | Source → destination |
| --- | --- |
| Instruction | `FETCH.INSTRUCTION → DECODE.INSTRUCTION` |
| Current PC | `FETCH.PC → EXECUTE.PC` |
| Register operands | `DECODE.RS1_VALUE / RS2_VALUE → EXECUTE` |
| Memory address | `EXECUTE.ALU_RESULT → MEMORY.ADDR` |
| Store data | `DECODE.RS2_VALUE → MEMORY.WRITE_DATA` |

Decode also supplies the immediate and controls for execution, memory access, and writeback. These connections are specified in [4.4.7](core.md); the role diagram is not a complete wiring diagram.

### Writeback sources

<!-- diagram: single-cycle-writeback -->

### Next PC selection

<!-- diagram: single-cycle-next-pc -->

### Completion control

<!-- diagram: single-cycle-completion -->

`EXECUTION_STATUS` drives the two stages' ISSUE inputs as direct read/access enables. Memory.COMMIT is tied high: ISSUE excludes prior faults, the stage checks alignment, and the endpoint rejects invalid stores. A response fault must not change the request opcode. DONE is tied high in both direct stages; FAULT and alignment still affect completion. [4.4.7](core.md) specifies all control pins and wiring.

### State ownership

| State | Owner | When it changes |
| --- | --- | --- |
| Architectural PC | `FETCH.PC_STATE` | Reset or an authorized completion edge |
| x1–x31; x0 is constant zero | `DECODE.REGISTER_FILE` | Reset or an authorized destination write |
| Halt/trap flags and cause | `EXECUTION_STATUS` | Terminal attempt edge; reset |
| Instruction/data storage | External zero-wait FF endpoints | Provisioning before simulation; data writes on the CPU rising edge |

Execute, Memory, Writeback, and Next PC are combinational. No stage owns a private instruction/data array. Memory request registers belong to the advanced pipeline's transaction interface, not this single-cycle datapath or the baseline pipeline.

## ALU sharing

`EXECUTE.ALU` performs register/immediate arithmetic, effective-address addition, branch comparisons, LUI/AUIPC, and JAL/JALR target addition. A branch needs comparison and `PC+IMM` concurrently, so `EXECUTE.BRANCH_TARGET_ADD` is separate. `FETCH.PC_PLUS_4_ADD` independently supplies fallthrough and the link value. There is no duplicate `RS1+IMM` jump adder.

## No reading back

An instruction does not modify architectural state and then read that new value to finish itself. Source operands, effective addresses, targets, link values, and store payloads use pre-edge state. On a normal edge, PC, destination register, and any data-memory write capture their already-computed inputs together. Stores have no acknowledgment or read-back phase.

For `jalr x5,0(x5)` at PC=16 with old x5=24, Execute computes target 24 while Writeback selects link 20. The edge writes PC=24 and x5=20. The updated x5 is not used to compute the target. For `lw x1,0(x1)`, old x1 supplies the address and changes only at the edge after the read has settled.

## Modeling boundary

Stage equivalence covers settled binary values and clock-edge state transitions after reset. Output times follow one rule: a behavioral part publishes each output after the delay proven on its gate-level form: never earlier than the gates, and later only where the gates happen to be quicker than their worst case. Beyond that, arbitrary X/Z behavior is not claimed equivalent. The examples omit events, delays, and history storage; the per-stage timing notes state the nonbinary limitations. Clock/control/data setup must precede capture. These are simulator contracts, not a calibrated physical timing or metastability model.
