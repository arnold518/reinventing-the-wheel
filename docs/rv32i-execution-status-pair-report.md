# RV32I Execution Control/Status Pair Report

Date: 2026-07-19

## Outcome

Block 5 is implemented as an independent structural/behavioral pair:

- `RV32IExecutionControlStatusUnit` is the educational implementation. Its decisions are built from visible gates, 4-bit muxes, bit adapters, constants, and `MemoryBit` state cells.
- `BehavioralRV32IExecutionControlStatusUnit` is the compact answer sheet. It calculates the same contract directly and stores only its own halt/trap state.
- `RV32IExecutionControlStatusUnitPairTest` drives both components from the same wires and checks independent expected values plus pair equality at 30 labeled checkpoints.

The structural component does not instantiate or call the behavioral component, the instruction oracle, or `BehavioralRV32ICore`.

## Why This Is A Real Module

This block is more than wiring. It owns policy and state that no other block should duplicate:

- whether the current instruction has enough instruction/data-memory readiness to attempt completion
- whether a memory request must remain active while data memory is waiting
- whether PC and register writes are permitted
- load/store address-alignment classification
- precise priority when several faults or requests are asserted together
- latched `HALTED`, `TRAPPED`, and `TRAP_CAUSE`

The memory data path itself stays direct: the ALU provides the address, `rs2` provides store data, decoded size/sign controls go to memory, and memory read data goes to the writeback mux.

## External Contract

Inputs:

| Pin | Width | Meaning |
| --- | ---: | --- |
| `CLK` | 1 | Rising edge latches a halt or trap event. |
| `RST` | 1 | Clears halted/trapped state and cause. |
| `ENABLE` | 1 | Allows an otherwise active CPU to attempt an instruction. |
| `LEGAL` | 1 | Says whether decode recognized a legal instruction. `0` always means illegal instruction. |
| `REG_WRITE` | 1 | Raw decode intent to write `rd`. |
| `MEM_READ`, `MEM_WRITE` | 1 each | Raw load/store intent. |
| `HALT_REQUEST` | 1 | Raw EBREAK-style halt request. |
| `TRAP_REQUEST` | 1 | Raw decoder trap request, currently ECALL. |
| `DECODE_TRAP_CAUSE` | 4 | Decoder-provided cause when `LEGAL=1` and `TRAP_REQUEST=1`. |
| `ALU_ADDRESS` | 32 | Effective load/store address used by the alignment checker. |
| `MEM_SIZE` | 2 | Byte=`0`, halfword=`1`, word=`2`; `3` is invalid/misaligned. |
| `PC_MISALIGNED` | 1 | Current instruction address is not 4-byte aligned. |
| `TARGET_MISALIGNED` | 1 | A taken branch/jump candidate is not 4-byte aligned. |
| `IMEM_READY`, `IMEM_FAULT` | 1 each | Instruction-memory response. |
| `DMEM_READY`, `DMEM_FAULT` | 1 each | Data-memory response. |

Outputs:

| Pin | Width | Meaning |
| --- | ---: | --- |
| `PC_WRITE` | 1 | Final permission to commit the next PC. |
| `REGISTER_WRITE` | 1 | Final permission to write `rd`. |
| `MEMORY_REQUEST_ACTIVE` | 1 | Keeps a legal load/store request presented to data memory. |
| `HALTED` | 1 | Latched halt state. |
| `TRAPPED` | 1 | Latched trap state. |
| `TRAP_CAUSE` | 4 | Latched `RV32IExecutionTrapCause` encoding. |
| `DATA_ADDRESS_MISALIGNED` | 1 | Raw address/size alignment classification. |
| `INSTRUCTION_ATTEMPT` | 1 | The active instruction has the readiness required to make its priority decision. |

## Memory Handshake Rule

For a legal active load/store, `MEMORY_REQUEST_ACTIVE` becomes `1` before data memory reports its response. It stays `1` when `DMEM_READY=0`, while `INSTRUCTION_ATTEMPT`, `PC_WRITE`, and `REGISTER_WRITE` stay `0`.

The request does not depend combinationally on `DMEM_FAULT`. Otherwise the CPU would remove the request that the memory needs in order to report that fault. Once `DMEM_READY=1`, the fault participates in the commit/trap decision.

## Alignment Logic

The structural checker uses only `ALU_ADDRESS[1:0]` and `MEM_SIZE[1:0]`:

| Size | Required low address bits | Misaligned when |
| --- | --- | --- |
| Byte (`0`) | none | never |
| Halfword (`1`) | bit 0 = 0 | `address[0]=1` |
| Word (`2`) | bits 1:0 = 00 | either low bit is 1 |
| Invalid (`3`) | none | always |

The raw `DATA_ADDRESS_MISALIGNED` output remains visible even after halt/trap. Architectural permissions still remain suppressed by the latched state.

## Priority

After reset/previous-state suppression and memory-ready waiting, exactly one result wins:

1. current PC misalignment
2. instruction-memory access fault
3. decode trap or illegal instruction
4. halt request
5. data-address misalignment
6. data-memory access fault
7. taken target misalignment
8. normal completion

`LEGAL=0` forces the illegal-instruction cause even if `TRAP_REQUEST` and another decoder cause are inconsistent. This makes the boundary fail safely.

Trap-cause values are the ordinal `RV32IExecutionTrapCause` contract:

| Value | Cause |
| ---: | --- |
| 0 | none |
| 1 | illegal instruction |
| 2 | environment call |
| 3 | instruction address misaligned |
| 4 | instruction access fault |
| 5 | load address misaligned |
| 6 | load access fault |
| 7 | store address misaligned |
| 8 | store access fault |

## Structural Internals

The structural implementation contains:

- two `MemoryBit` cells for halted/trapped state
- four `MemoryBit` cells for the 4-bit trap cause
- address/size splitters and a cause joiner/splitter
- visible byte/halfword/word alignment gates
- an active/request/readiness gate chain
- one visible priority stage per fault/request class
- structural 4-bit cause muxes
- final PC/register permission gates

`Mux2to1_4bit` was added as a reusable structural helper. It is itself built from four gate-level `Mux2to1` lanes and has an independent truth-table test.

## Test Coverage

The pair test checks:

- reset clearing all state and permission outputs
- normal instruction completion
- `ENABLE=0`
- instruction-memory wait
- data-memory wait with request persistence
- ready load completion
- PC-misalignment priority over simultaneous instruction/decode/halt/data/target conditions
- instruction access fault
- illegal-instruction safety when decode signals disagree
- ECALL trap cause
- EBREAK halt state
- misaligned halfword load and word store
- load and store access faults
- target misalignment
- data misalignment winning over data access fault and target misalignment
- post-halt/post-trap suppression
- reset recovery before every stateful scenario

Every checkpoint checks all eight output contracts against hard-coded expected values for both implementations and then compares the two outputs directly.

## Visualization

The browser visualizer keeps the implementations in separate scenarios:

- `rv32i-status-structural`: one blue, expandable `RV32IExecutionControlStatusUnit` root showing the real alignment, priority, cause-selection, and state-storage hierarchy.
- `rv32i-status-behavioral`: one amber `BehavioralRV32IExecutionControlStatusUnit` root showing the compact answer-sheet contract.

The standalone scenarios use a concise eight-checkpoint teaching waveform. The 30-checkpoint `RV32IExecutionControlStatusUnitPairTest` remains in CTest as the deeper equivalence regression and is intentionally absent from the browser selector.

## Known Boundary

Interchangeability is proven for known binary CPU inputs. The behavioral component deliberately returns conservative unknown combinational outputs when required inputs or stored state are unknown. The structural gates may sometimes preserve a more specific partial result. CPU bring-up uses known binary inputs, so this boundary does not block integration.
