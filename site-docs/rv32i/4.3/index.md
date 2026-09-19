# § 4.3. Single-Cycle Core

`RV32ISingleCycleCore` connects six logical datapath stages and a shared execution-status unit. Each stage has structural and behavioral implementations under the same pin contract. [4.1](../4.1/index.md) specifies the instructions; [4.2](../4.2/index.md) specifies the reused building blocks.

“Single-cycle” names the non-pipelined datapath, not a promise that every instruction finishes on the next clock. The universal memory ports use clocked request/response transactions, so fetches and loads/stores can require several clocks. There are no IF/ID/EX/MEM/WB pipeline registers and no overlapped instructions.

## Components

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32ISingleCycleFetchStage` | PC storage, PC+4, instruction transaction, fetch status | Yes | Yes |
| `RV32ISingleCycleDecodeStage` | Instruction decoding and the architectural register file | Yes | Yes |
| `RV32ISingleCycleExecuteStage` | Shared ALU, operand selection, branch comparison, jump/branch targets | Yes | Yes |
| `RV32ISingleCycleMemoryStage` | Data transaction and load/store lane formatting | Yes | Yes |
| `RV32ISingleCycleWritebackStage` | Select the value to write to a destination register | Yes | Yes |
| `RV32ISingleCycleNextPCStage` | Select the next PC and check an active transfer's alignment | Yes | Yes |
| `RV32ISingleCycleCore` | Connect the six stages, execution status, and two external memory ports | Yes | Yes |

Columns describe registered simulator implementations, not the simplified C++ examples. `RV32IExecutionControlStatusUnit` is reused from [4.2.7](../4.2/execution-status.md), with both implementations; it is not a seventh datapath stage.

## Contents

| Section | Coverage |
| --- | --- |
| [4.3.1. Fetch](fetch.md) | PC, PC+4, request capture, response lifetime, reset |
| [4.3.2. Decode and Register Read](decode.md) | Decoding controls, two reads, one completion-edge write, x0 |
| [4.3.3. Execute](execute.md) | Operand selectors, one shared ALU, branch conditions, target generation |
| [4.3.4. Memory Access](memory.md) | Transaction pins, byte lanes, sign extension, faults, backpressure |
| [4.3.5. Writeback](writeback.md) | Zero/ALU/load/link selection; no storage |
| [4.3.6. Next PC](next-pc.md) | Fallthrough versus transfer, target alignment; no PC storage |
| [4.3.7. Complete Core](core.md) | Full wiring, completion, memory endpoints, instruction examples and tests |

Each stage page follows **IO pins → behavior and C++ example → structural/behavioral implementation → examples and embedded simulation → tests**. Its scrollable example table contains all recorded rows, including checkpoint numbers and simulator times. The C++ examples reuse explicitly linked 4.2 models rather than redefining the decoder, ALU, or transaction protocol.

## Datapath and state ownership

```text
instruction endpoint ⇄ FETCH ── instruction ──► DECODE ── operands/control ──► EXECUTE
                       │ PC, PC+4                ▲                            │
                       │                         │ WB_VALUE                   │ address/result
                       │                      WRITEBACK ◄── load value ─── MEMORY ⇄ data endpoint
                       │                         ▲
                       └──────── PC+4 ───────────┤
                                                 └── ALU result (from EXECUTE)

FETCH ◄── NEXT_PC ◄── PC+4 / control target / branch and jump selection
  ▲          ▲
  └── completion enables and status from EXECUTION_STATUS ──► DECODE / MEMORY
```

| State | Owner | When it changes |
| --- | --- | --- |
| Architectural PC | `FETCH.PC_STATE` | Reset or an authorized completion edge |
| x1–x31; x0 is constant zero | `DECODE.REGISTER_FILE` | Reset or an authorized destination write |
| Pending request fields and progress | `FETCH.REQUEST`, `MEMORY.REQUEST` | Request/response protocol edges; reset |
| Halt/trap flags and cause | `EXECUTION_STATUS` | Terminal attempt edge; reset |
| Instruction/data storage | External memory endpoints | Their own initialization, write, and reset contracts |

Execute, Writeback, and Next PC are combinational. Request payload registers are protocol state, not pipeline registers. No stage owns a second register file or a private instruction/data memory.

## ALU sharing

`EXECUTE.ALU` performs register/immediate arithmetic, effective-address addition, branch comparisons, LUI/AUIPC, and JAL/JALR target addition. A branch needs comparison and `PC+IMM` concurrently, so `EXECUTE.BRANCH_TARGET_ADD` is separate. `FETCH.PC_PLUS_4_ADD` independently supplies fallthrough and the link value. There is no duplicate `RS1+IMM` jump adder.

## No reading back

An instruction does not modify an architectural register or PC and then read that new value to finish itself. Source operands, effective addresses, targets, and link values use the pre-completion state. On a normal completion edge, the PC and destination register capture their already-computed inputs together. Stores wait for acknowledgment, not for a read-back of the data they wrote.

For `jalr x5,0(x5)` at PC=16 with old x5=24, Execute computes target 24 while Writeback selects link 20. The edge writes PC=24 and x5=20. The updated x5 is not used to compute the target. For `lw x1,0(x1)`, x1 retains the address throughout the wait and changes only when the load completes.

## Modeling boundary

Stage equivalence covers settled binary values and clock-edge state transitions after reset. Structural gate delays and behavioral one-tick outputs need not match, and arbitrary X/Z behavior is not claimed equivalent. The examples omit events, delays, and history storage; the per-stage timing notes state the nonbinary limitations. Clock/control/data setup must precede capture. These are simulator contracts, not a calibrated physical timing or metastability model.
