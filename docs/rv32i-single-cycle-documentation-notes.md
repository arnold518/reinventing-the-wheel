# Single-cycle core: supporting reference notes

The published stage and core references are now [§4.3](../site-docs/rv32i/4.3/index.md). This working note retains the core-specific material and source markers originally separated from §4.1.4, including the host/reference APIs checked by the documentation tests. The public instruction-level reference remains [§4.1](../site-docs/rv32i/4.1/index.md).

The pin table remains checked against the implementation by `RV32IReferenceDocumentationTest`. Source markers are retained for source validation and internal API reference.

## Execution organization

The structural core now has six independently selectable stage components. These are functional responsibilities, not six clock cycles or six pipeline registers. See [the stage implementation reference](rv32i-single-cycle-stages.md) for interfaces, state ownership, ALU sharing, and the no-reading-back contract.

| Phase | Inputs / work | Result | Current implementation |
| --- | --- | --- | --- |
| Fetch | Current PC, instruction endpoint | Instruction, PC, PC+4, fetch status | `FETCH`: PC storage, incrementer, requester |
| Decode / register read | Word and register state | Source values and execution controls | `DECODE.CONTROL` and `DECODE.REGISTER_FILE` |
| Execute | Source values, immediate, old PC | ALU result, address, branch condition, control target | `EXECUTE`: one shared ALU, operand muxes, concurrent branch-target adder |
| Memory | Effective address, width, store value, data endpoint | Load value and transaction status | `MEMORY`: lane formatting and requester; storage remains external |
| Writeback | ALU result, load result, PC+4 | Register write value | `WRITEBACK`: result mux; Decode owns the register-file write port |
| Next PC | PC+4, control target, branch/jump selection | Candidate next PC and target alignment | `NEXT_PC`: selection and alignment; Fetch owns PC capture |

`EXECUTION_STATUS` separately authorizes completion and records terminal status. The C++ instruction oracle still implements the same architectural transition within `stepImpl()`, with helper functions; it is not the implementation of the six behavioral stage components. The functional store is applied after its checks and before the final PC assignment; a store has no later branch-target check. Transaction memories timestamp a store when the endpoint applies it; the CPU retires it only after its response.

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

All words are 32 bits unless a different width is listed. Pin values are binary at supported execution checkpoints.

| Pin | Direction | Width | Contract |
| --- | --- | ---: | --- |
| `CLK` | Input | 1 | Known LOW→HIGH transition attempts an instruction when permitted |
| `RST` | Input | 1 | High resets core state; reset has priority and does not require a rising edge |
| `ENABLE` | Input | 1 | High permits execution; low holds architectural state |
| `IMEM_REQ_VALID` | Output | 1 | Held until request acceptance |
| `IMEM_REQ_READY` | Input | 1 | Endpoint can accept the request |
| `IMEM_REQ_OP` | Output | 3 | Read operation (0) |
| `IMEM_REQ_ADDR` | Output | 32 | Byte address of the accessed word |
| `IMEM_REQ_WRITE_DATA` | Output | 32 | Zero (fetch is a read) |
| `IMEM_REQ_WRITE_MASK` | Output | 4 | All byte lanes |
| `IMEM_RSP_VALID` | Input | 1 | Endpoint response is available |
| `IMEM_RSP_READY` | Output | 1 | Core acknowledges or discards the response |
| `IMEM_RSP_READ_DATA` | Input | 32 | Whole word; subword lane selection occurs inside the CPU |
| `IMEM_RSP_STATUS` | Input | 3 | Zero for success; nonzero becomes access fault |
| `DMEM_REQ_VALID` | Output | 1 | Held until request acceptance |
| `DMEM_REQ_READY` | Input | 1 | Endpoint can accept the request |
| `DMEM_REQ_OP` | Output | 3 | Read (0) or ordinary write (1) |
| `DMEM_REQ_ADDR` | Output | 32 | Byte address of the accessed word |
| `DMEM_REQ_WRITE_DATA` | Output | 32 | Store value shifted into its byte lanes |
| `DMEM_REQ_WRITE_MASK` | Output | 4 | One write enable per byte |
| `DMEM_RSP_VALID` | Input | 1 | Endpoint response is available |
| `DMEM_RSP_READY` | Output | 1 | Core acknowledges or discards the response |
| `DMEM_RSP_READ_DATA` | Input | 32 | Whole word; subword lane selection occurs inside the CPU |
| `DMEM_RSP_STATUS` | Input | 3 | Zero for success; nonzero becomes access fault |
| `PC` | Output | 32 | Current architectural PC |
| `HALTED` | Output | 1 | Latched project halt status |
| `TRAPPED` | Output | 1 | Latched terminal trap status |
| `TRAP_CAUSE` | Output | 4 | Project cause encoding |
| `INSTRUCTION_ATTEMPT` | Output | 1 | Permission to commit the current attempt at the next rising edge |

Both ports connect directly to `MemoryContainer` or a complete `MemoryEndpoint`. CLK/RST are shared. Request acceptance and response completion are different edges. The internal `RV32IMemoryLanes` handles width/sign extension and byte masks; `RV32IMemoryRequest` holds each request stable and tracks obsolete fetches. See [direct memory integration](rv32i-memory-integration.md).

The compact `RV32ISingleCycleSystem` has only six pins: inputs `CLK`, `RST`, `ENABLE` and outputs `PC<32>`, `HALTED`, `TRAPPED`. Its memory and full architectural state are accessed through APIs. It has **no** `TRAP_CAUSE` or external memory pin. The visual `RV32IProgramRoot` is a different testbench: it contains an internal clock and exposes `RST`, `ENABLE`, `PC`, `HALTED`, `TRAPPED`, and `TRAP_CAUSE` while leaving both memories visible.

## Clock, memory, and unknown-input behavior

The core is active only with `RST=0`, `ENABLE=1`, and neither terminal status set. It waits for the matching instruction response; a load/store additionally waits for the matching data response. REQ_VALID remains asserted until acceptance, not until completion. Endpoint RSP_VALID/data/status stay stable while RSP_READY is low. The attached memory must reject a faulting store rather than write partial data. Instruction/memory response signals must settle before the active edge.

The reference computes the instruction result directly on a permitted rising edge, then publishes outputs with its one abstract simulator-delay tick. That tick is **not** gate propagation, a measured hardware latency, or the 4000-unit clock period used by the program fixtures. Falling edges and input-only reevaluations can update request outputs but do not execute another instruction.

The registered whole-core contract is **known-binary, clock-boundary** equivalence. It is not a four-state-equivalent replacement for every internal gate. Unknown/high-impedance control bits are sanitized to unknown; an unknown clock transition is not a recognized LOW→HIGH edge. Unknown instruction data suppresses an ordinary attempt; a ready fetch-fault response can still terminate without a valid instruction word. In this behavioral single-cycle reference, a load attempting to consume unknown selected data bits raises a simulator logic error, not an architectural RISC-V exception. Unknown neighboring byte lanes are ignored by byte/halfword loads. Do not interpret gate-level X/Z propagation as part of the binary ISA answer sheet.

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
