# RV32I Milestone 6 Report: Instruction Oracle And Trace

Last updated: 2026-06-04

## Status

Milestone 6 is implemented.

This milestone adds a pure C++ RV32I reference model:

```text
RV32IProgram / bytes in memory
  -> fetch instruction at pc
  -> RV32IDecoder
  -> RV32IControl
  -> execute instruction against pc, registers, and memory
  -> emit one trace row
```

It is not a `Component`, `IOComponent`, or `BasicComponent`. It has no pins, wires, simulator events, layout, or visualizer scenario.

Its job is to become the golden behavior source for later CPU/component tests.

## Implemented Files

Instruction-oracle API:

- `include/rv32i/RV32IState.hpp`
- `include/rv32i/RV32IFunctionalMemory.hpp`
- `include/rv32i/RV32IInstructionTrace.hpp`
- `include/rv32i/RV32IInstructionOracle.hpp`

Instruction-oracle implementation:

- `src/rv32i/RV32IFunctionalMemory.cpp`
- `src/rv32i/RV32IInstructionOracle.cpp`

Tests:

- `include/tests/RV32IInstructionOracleTests.hpp`
- `src/tests/RV32IInstructionOracleTests.cpp`

Build and registry:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/tests/TestRegistry.cpp`

## What It Uses From Earlier Milestones

Milestone 3:

- `RV32IDecoder`
- decoded instruction fields
- RV32I instruction enum
- immediate reconstruction
- illegal-instruction detection

Milestone 4:

- `RV32IControl`
- ALU operation selection
- register-write intent
- memory read/write intent
- memory size and load sign-extension intent
- branch/jump intent
- halt/trap intent

Milestone 5:

- `RV32IProgram`
- raw program bytes
- little-endian word packing

Milestone 6 adds the part that was still missing:

- architectural registers
- functional memory semantics
- instruction execution
- per-instruction trace output
- run-until-halt/trap/instruction-limit behavior

## State Model

`RV32IState` contains:

| Field | Meaning |
| --- | --- |
| `pc` | Current byte address of the instruction being fetched. |
| `x[32]` | Architectural integer registers. |
| `halted` | True after project-level halt, currently `EBREAK`. |
| `trapped` | True after illegal instruction, `ECALL`, memory fault, fetch fault, or instruction limit. |
| `trap_cause` | Instruction-oracle trap reason. |
| `instruction_count` | Number of attempted instruction executions. |

Register rule:

- `x0` is always forced to zero.
- Writes to `x0` are recorded in the trace as ignored, but architectural state remains zero.

## Functional Memory Model

`RV32IFunctionalMemory` is a byte-addressed little-endian memory used by the instruction oracle.

Default capacity:

- 256 KiB
- same byte capacity as `BehavioralMemory64Kx32`

Supported setup/debug API:

| API | Behavior |
| --- | --- |
| `loadProgram(program, base)` | Loads `RV32IProgram` bytes at a byte address. |
| `loadBytes(base, data)` | Writes raw bytes. |
| `loadWords(base, words)` | Writes 32-bit words as little-endian bytes; base must be aligned. |
| `readBytes(base, count)` | Reads raw bytes. |
| `readU8/U16/U32` | Reads little-endian values. |
| `writeU8/U16/U32` | Writes little-endian values. |

Instruction-oracle memory rules:

| Instruction | Rule |
| --- | --- |
| `LB` | Read one byte and sign-extend to 32 bits. |
| `LBU` | Read one byte and zero-extend. |
| `LH` | Read two bytes, require 2-byte alignment, sign-extend. |
| `LHU` | Read two bytes, require 2-byte alignment, zero-extend. |
| `LW` | Read four bytes, require 4-byte alignment. |
| `SB` | Store low 8 bits of `rs2`. |
| `SH` | Store low 16 bits of `rs2`, require 2-byte alignment. |
| `SW` | Store all 32 bits of `rs2`, require 4-byte alignment. |

Fault policy:

- Misaligned instruction fetch traps.
- Out-of-range instruction fetch traps.
- Misaligned load/store traps.
- Out-of-range load/store traps.
- Faulted stores do not modify memory.

This matches the current project memory-component policy, where halfword/word misalignment is a fault.

## Trace Model

Each successful or faulting instruction execution produces one `RV32IInstructionTrace`.

Important fields:

| Field | Meaning |
| --- | --- |
| `instruction_index` | Architectural instruction number. |
| `pc_before` | PC used for instruction fetch. |
| `raw_instruction` | Fetched 32-bit instruction word. |
| `decoded` | `RV32IDecodedInstruction` from Milestone 3. |
| `control` | `RV32IControlSignals` from Milestone 4. |
| `rs1`, `rs2`, `rd` | Register field IDs. |
| `rs1_value`, `rs2_value` | Source register values before writeback. |
| `immediate` | Decoded immediate. |
| `alu_a`, `alu_b`, `alu_result` | Functional ALU inputs and result. |
| `memory` | Memory access kind, size, address, data, and fault flag. |
| `writeback` | Register writeback register/value and x0 ignore flag. |
| `branch_taken` | Branch decision for branch instructions. |
| `pc_after` | PC after the instruction. |
| `halted`, `trapped`, `trap_cause` | End-of-instruction stop state. |

This trace is the important output for future component comparison. We are not adding a separate cycle oracle now. Component tests should compare at instruction commit boundaries: after the component reports that one instruction has committed, advance `RV32IInstructionOracle` by one instruction and compare architectural state.

## Instruction Behavior

Implemented RV32I base behavior:

- R-type ALU:
  - `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`, `OR`, `AND`
- I-type ALU:
  - `ADDI`, `SLTI`, `SLTIU`, `XORI`, `ORI`, `ANDI`, `SLLI`, `SRLI`, `SRAI`
- Loads:
  - `LB`, `LH`, `LW`, `LBU`, `LHU`
- Stores:
  - `SB`, `SH`, `SW`
- Branches:
  - `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`
- Jumps:
  - `JAL`, `JALR`
- Upper immediates:
  - `LUI`, `AUIPC`
- System/control:
  - `FENCE` is a no-op that advances `pc`.
  - `ECALL` traps with `EnvironmentCall`.
  - `EBREAK` halts and keeps `pc` at the `EBREAK` instruction.
  - illegal encodings trap with `IllegalInstruction`.

PC behavior:

| Instruction class | `pc_after` |
| --- | --- |
| Normal ALU/load/store/fence | `pc + 4` |
| Branch not taken | `pc + 4` |
| Branch taken | `pc + immediate` |
| `JAL` | `pc + immediate` |
| `JALR` | `(rs1 + immediate) & ~1` |
| `EBREAK` | current `pc` |
| Trap | current `pc` |

## Tests

New test:

- `RV32IInstructionOracleTest`

Covered behavior:

- simple three-instruction program trace: `ADDI`, `ADDI`, `EBREAK`
- `x0` write-ignore behavior
- all R-type ALU operations
- all I-type ALU operations
- load sign-extension and zero-extension
- store byte/halfword/word lane behavior
- branch taken and not-taken behavior
- `JAL` return address and target
- `JALR` return address and low-bit clearing
- `LUI`
- `AUIPC`
- `FENCE`
- `ECALL`
- `EBREAK`
- illegal instruction trap
- misaligned instruction fetch trap
- misaligned load trap
- out-of-range store trap
- instruction-limit trap

Focused verification command:

```bash
ctest --test-dir build --output-on-failure -R "RV32IInstructionOracleTest|RV32IProgramLoaderTest|RV32IControlTest|RV32IDecoderTest"
```

Result:

- Passed: 4/4
- Time: 0.05 seconds

Full regression command:

```bash
ctest --test-dir build --output-on-failure
```

Result:

- Passed: 90/90
- Time: 1095.44 seconds

## 7A Follow-Up: Instruction-Lockstep Harness

Milestone 7A adds the reusable component-vs-oracle test harness that this report originally deferred.

Implemented files:

- `include/tests/RV32IInstructionLockstepTests.hpp`
- `src/tests/RV32IInstructionLockstepTests.cpp`

Supporting simulator/test infrastructure:

- `Simulator::advanceAndRecord(target_time)` advances the event simulation incrementally without clearing recorded history.
- `SimulationTest::runSimulation()` is a virtual hook; existing tests keep the default batch run.
- `SimulatorAdvanceAndRecordTest` verifies that incremental advancement preserves future events.
- `RV32IInstructionLockstepHarnessTest` verifies the lockstep harness with a fake RV32I component that commits through `RV32IInstructionOracle`.

Harness shape:

```text
same program + same initial state
  -> component CPU runs until it commits instruction N
  -> RV32IInstructionOracle executes instruction N
  -> compare architectural state
  -> compare logical data-memory access
  -> compare actual data-memory writes for instruction N
  -> repeat until halt, trap, or instruction limit
```

This is instruction-lockstep testing, not cycle-by-cycle testing. It stays reusable for single-cycle, multi-cycle, and later pipelined components because it checks architectural commit behavior rather than internal timing.

Milestone 6 keeps that plan in mind by emitting rich instruction trace rows. Milestone 7A now defines:

- `RV32IInstructionLockstepTest`
- `RV32ISystemProgramCase`
- component-state adapter method
- `lastDataMemoryAccess()` adapter method
- `lastDataMemoryWrites()` adapter method
- data initialization helper records

The real component CPU still needs to implement the commit signal/getter contract.

## What This Does Not Implement

Milestone 6 does not implement:

- a circuit component CPU
- simulator pins or clocks
- visualizer integration
- cycle oracle
- cycle-by-cycle component timing checks
- ELF loading
- external assembler workflow
- compressed instructions
- privileged mode
- interrupts
- pipeline behavior
- cache behavior

## Next Step

The next practical step is to build a reusable behavioral RV32I system component:

- `BehavioralRV32ICore`
- `RV32ISystem`
- owned instruction and data memories
- external `CLK`, `RST`, and `ENABLE`

Those component tests should compare against the Milestone 6 instruction oracle at instruction commit boundaries, not only at final state. A separate cycle oracle is not planned for this stage.
