# Single-cycle core: supporting reference notes

The published stage and core references are now [§4.4](../site-docs/rv32i/4.4/index.md). This working note retains the core-specific material and source markers originally separated from §4.1.4, including the host/reference APIs checked by the documentation tests. The public instruction-level reference remains [§4.1](../site-docs/rv32i/4.1/index.md).

The published core pin table is checked against the implementation by `RV32IReferenceDocumentationTest`. Source markers are retained here for source validation and internal API reference.

## Execution organization

The structural core now has six independently selectable stage components. These are functional responsibilities, not six clock cycles or six pipeline registers. See [the stage implementation reference](rv32i-single-cycle-stages.md) for interfaces, state ownership, ALU sharing, and the no-reading-back contract.

| Phase | Inputs / work | Result | Current implementation |
| --- | --- | --- | --- |
| Fetch | Current PC, native instruction memory | Instruction, PC, PC+4, fetch status | `FETCH`: PC storage, incrementer, direct read |
| Decode / register read | Word and register state | Source values and execution controls | `DECODE.CONTROL` and `DECODE.REGISTER_FILE` |
| Execute | Source values, immediate, old PC | ALU result, address, branch condition, control target | `EXECUTE`: one shared ALU, operand muxes, concurrent branch-target adder |
| Memory | Effective address, width, store value, native data memory | Load value and access faults | `MEMORY`: combinational lanes and store permission; storage remains external |
| Writeback | ALU result, load result, PC+4 | Register write value | `WRITEBACK`: result mux; Decode owns the register-file write port |
| Next PC | PC+4, control target, branch/jump selection | Candidate next PC and target alignment | `NEXT_PC`: selection and alignment; Fetch owns PC capture |

`EXECUTION_STATUS` separately authorizes completion and records terminal status. The C++ instruction oracle still implements the same architectural transition within `stepImpl()`, with helper functions; it is not the implementation of the six behavioral stage components. The functional store is applied after its checks and before the final PC assignment; a store has no later branch-target check. In the single-cycle circuit, memory writes and architectural writes capture together at the rising edge.

For a successful `lw x5, 4(x6)`: fetch its word, read `x6` and offset `4`, calculate the address, read the word, then commit `x5` and PC+4. A load fault suppresses the normal register and PC updates.

The structural core uses connected circuit blocks, so independent calculations can occur in parallel as signals settle. State changes are accepted at the active clock edge when enabled and ready. The behavioral model computes the same binary clock-boundary effects directly. Neither organization implies one clock cycle per row of the table.

## Public component identity

Select **behavioral fidelity** of `RV32ISingleCycleCore` to run the clocked reference against external memories. Internally its C++ class is `RV32IReferenceCore`, a private factory implementation. Select behavioral `RV32ISingleCycleSystem` for a compact system with owned memories; its private class is `RV32IReferenceSystem`. Neither is a separate public ISA or a second set of component pins.

The pure `RV32IInstructionOracle::step(state, imem, dmem)` is the semantic engine used by these adapters. It uses `RV32IDecoder`, `RV32IControl`, and explicit memory/state objects; it does not instantiate a gate-level datapath. This is an executable reference for structural construction and testing, not a replacement for that construction.

| Reference API | Inputs | Result / side effects |
| --- | --- | --- |
| `RV32IDecoder::decode(word)` | One 32-bit instruction word | Decoded fields, immediate, instruction/format enums, legality, and diagnostic status; no state changes |
| `RV32IControl::fromDecoded(decoded)` | Decoder result | Operand-use, ALU, writeback, memory, and control-flow selections; no state changes |
| `RV32IInstructionOracle::step(state, imem, dmem)` | Mutable state and two functional memories | At most one attempted instruction; returns `RV32IInstructionTrace` and updates state/data memory |
| `RV32IInstructionOracle::step(state, memory)` | Mutable state and one functional memory | Same transition with unified instruction/data storage |
| `RV32IInstructionOracle::run(state, memory, limit)` | Unified memory and maximum attempts | Repeated steps; returns final state, trace list, and limit-reached indicator |
| `snapshotArchitecturalState()` | Core/system component | Current architectural registers, PC, and status through the capability API; not a pin for each register |
| System `loadProgram`, `loadInstructionBytes`, `loadDataBytes`, `readDataBytes` | Program image / byte range | Host preload/readback, outside instruction execution; invalid host ranges raise host errors |

The internal `stepWithoutDataMemoryWrite()` variant computes the same instruction transition but returns stores as trace effects without mutating data memory. The behavioral system adapter then applies those stores with simulator timestamps. It is not the ordinary API for running a complete standalone program, because ignoring its returned store effects would lose writes.

## Core IO pins

See the authoritative [complete core IO table](../site-docs/rv32i/4.4/core.md#io-pins). Both memory ports use the universal byte-addressed REQ/RSP interface, not the old native word-address pins. Pin values are binary at supported execution checkpoints.

Both ports connect to zero-wait FF memory endpoints. Their controllers translate requests to native FF storage operations without adding a memory cycle. CLK is shared; native storage reset is tied low so CPU reset preserves images. Two combinational `RV32IZeroWaitMemoryPort` groups translate the stages' native signals. `RV32IMemoryLanes` handles width/sign extension and byte masks. There is no `RV32IMemoryRequest` or pending transaction state in this core. See [memory integration](rv32i-memory-integration.md).

The compact `RV32ISingleCycleSystem` has only six pins: inputs `CLK`, `RST`, `ENABLE` and outputs `PC<32>`, `HALTED`, `TRAPPED`. Its memory and full architectural state are accessed through APIs. It has **no** `TRAP_CAUSE` or external memory pin. The visual `RV32IProgramRoot` is a different testbench: it contains an internal clock and exposes `RST`, `ENABLE`, `PC`, `HALTED`, `TRAPPED`, and `TRAP_CAUSE` while leaving both memories visible.

## Clock, memory, and unknown-input behavior

The core is active only with `RST=0`, `ENABLE=1`, and neither terminal status set. Endpoint responses and all combinational paths must settle before the active edge. For every active access, REQ_READY and RSP_VALID must both be asserted before that edge. This is a zero-wait endpoint contract, not support for stalling on a delayed response. Stores require final commit permission; access/alignment faults suppress stores and ordinary PC/register writes. The universal handshake adds no extra memory cycle here.

The reference computes the instruction result directly on a permitted rising edge, then publishes outputs with its one abstract simulator-delay tick. That tick is **not** gate propagation, a measured hardware latency, or the clock period chosen for a program fixture. Falling edges and input-only reevaluations can update memory request outputs but do not execute another instruction.

The registered whole-core contract is **known-binary, clock-boundary** equivalence. It is not a four-state-equivalent replacement for every internal gate. Unknown/high-impedance control bits are sanitized to unknown; an unknown clock transition is not a recognized LOW→HIGH edge. Unknown instruction data suppresses an ordinary attempt; an asserted instruction-memory fault can still terminate without a valid instruction word. In this behavioral single-cycle reference, a load attempting to consume unknown selected data bits raises a simulator logic error, not an architectural RISC-V exception. Unknown neighboring byte lanes are ignored by byte/halfword loads. Do not interpret gate-level X/Z propagation as part of the binary ISA answer sheet.

Reference-only host setters can preload a register or change reset PC. They are not general public hardware pins, and they do not imply that the structural core supports an arbitrary reset vector. The visual program fixtures use zero reset PC and zero initial registers.

## Clocked adapter source listings

??? example "Complete clocked behavioral core adapter"

    <!-- simulator-source: src/modules/rv32i/RV32IReferenceCore.hpp -->

    <!-- simulator-source: src/modules/rv32i/RV32IReferenceCore.cpp -->

??? example "Complete behavioral system adapter"

    <!-- simulator-source: src/modules/rv32i/RV32IReferenceSystem.hpp -->

    <!-- simulator-source: src/modules/rv32i/RV32IReferenceSystem.cpp -->

## Decoded controls are not instruction opcodes

`RV32IControl` produces project-local ALU selectors, operand selectors, writeback/memory controls, branch/jump kinds, and operand-use flags. It does not send `opcode=0x33` directly to `ALU32.OP`.

| Instruction class | ALU operands / operation | Register writeback | Other controls |
| --- | --- | --- | --- |
| Register ALU | `RS1`, `RS2`; selected arithmetic/logic operation | ALU result | Uses both sources and destination |
| Immediate ALU | `RS1`, immediate; selected operation | ALU result | No `rs2` dependency |
| LUI | Zero, upper immediate; `PASS_B` | ALU result | No source-register dependency |
| AUIPC | PC, upper immediate; `ADD` | ALU result | No source-register dependency |
| Load | `RS1`, immediate; `ADD` | Memory result | Read enable, width, extension |
| Store | `RS1`, immediate; `ADD` | None | Write enable/width; `RS2` supplies stored data |
| Branch | `RS1`, `RS2`; `SUB` in control mapping | None | Branch kind and immediate; oracle compares original operands directly |
| JAL | PC, immediate; `ADD` | `PC + 4` | Direct jump; no source-register dependency |
| JALR | `RS1`, immediate; `ADD` | `PC + 4` | Indirect jump and target-bit clearing |
| FENCE | Zero, zero; `ZERO` | None | No register or memory request |
| ECALL / rejected word | `ZERO` | None | Trap, with environment-call / illegal cause |
| EBREAK | Zero, zero; `ZERO` | None | Halt |

The destination-use flag describes the instruction form, not whether the destination is nonzero. `uses_rd`/`reg_write` may therefore be true for an instruction targeting `x0`; the actual register update is still suppressed. No dependency may be inferred from an unused raw field.
