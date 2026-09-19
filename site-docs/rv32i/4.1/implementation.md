# § 4.1.4. Executable RV32I Reference

[Reference index](index.md) · [Instruction catalogue](instructions/index.md) · [Programs and verification](programs/index.md)

The instruction entries specify the required effects in prose and pseudocode. This page documents the existing C++ model that computes those effects: `RV32IInstructionOracle`.

Its input is the current registers, PC, and memory. Its output is the next state and a record of the instruction's effects. It runs without a clock, circuit hierarchy, or visualizer. Later core implementations can be checked against it without adopting its software organization.

## Components

These are ordinary C++ types, not connected hardware components.

| Component / type | Description |
| --- | --- |
| `RV32IState` | Registers, PC, execution status, and attempted-instruction count |
| `RV32IFunctionalMemory` | Byte storage with program loading and little-endian reads/writes |
| `RV32IDecoder` | Instruction recognition and field/immediate extraction |
| `RV32IControl` | Internal software selection of instruction effects; it is not a hardware wiring specification |
| `RV32IInstructionOracle` | Executes one instruction or a bounded sequence |
| `RV32IInstructionTrace` | Record of the instruction, operands, memory access, destination write, next PC, and status |

## State and inputs

| Object / field | Type | Initial value or role |
| --- | --- | --- |
| `state.pc` | `uint32_t` | Defaults to zero; byte address of the next instruction to attempt |
| `state.x` | `std::array<uint32_t, 32>` | Defaults to zero; use `readRegister()` and `writeRegister()` to preserve the `x0` rule |
| `state.halted`, `state.trapped` | `bool` | Both default to false |
| `state.trap_cause` | `RV32IExecutionTrapCause` | Defaults to `None`; project exception enum |
| `state.instruction_count` | `uint64_t` | Defaults to zero; counts attempts, including terminal instructions and faults |
| `instruction_memory` | `RV32IFunctionalMemory` | Supplies instruction bytes at PC; preload before execution |
| `data_memory` | `RV32IFunctionalMemory` | Supplies load bytes and receives successful stores |

Functional memory defaults to 256 KiB of zero bytes; its constructor accepts a different byte count. `loadWords(address, words)` places instruction words in little-endian order. `loadBytes()`, `readBytes()`, and the `readU8/16/32()` / `writeU8/16/32()` methods provide host-side access.

Host memory methods check ranges; the instruction engine additionally applies the ISA/environment alignment and exception rules. A host loading error is a C++ exception, not a simulated instruction trap. See [machine conventions](machine.md) for address wrapping, memory policy, and trap meanings.

## Execution interface

| Call | Inputs | Result and side effects |
| --- | --- | --- |
| `step(state, imem, dmem)` | Mutable state and separate instruction/data memories | Attempts one instruction; updates state and successful data stores; returns one trace |
| `step(state, memory)` | Mutable state and one memory | Same operation with unified instruction/data storage |
| `run(state, memory, max_instructions)` | Mutable state, unified memory, and an attempt limit | Repeats steps until halt, trap, or limit; returns final state, traces, and a limit-reached flag |

All three calls are static methods of `RV32IInstructionOracle`. The three-argument `step()` is used for the separate-memory examples. The current `run()` helper has only the unified-memory form; it also mutates the supplied state and memory.

### One-step contract

For an active state, `step()` fetches and decodes the word at PC, reads the old operands, computes the instruction's effects, and applies the permitted updates. One call means one **instruction attempt**, not one hardware cycle.

- A successful instruction updates only its specified destination, memory bytes, and PC.
- A fault records its cause without changing the faulting instruction's destination, data memory, or PC.
- EBREAK uses the project's halt convention; ECALL produces an environment-call trap.
- Every active attempt increments `instruction_count` once.
- Calling `step()` after halt/trap does not execute another instruction or increment the count. The returned trace reports the stopped state, not a newly fetched instruction.
- Exhausting `run()`'s limit while still active sets `InstructionLimit` and `instruction_limit_reached=true`. This host stop adds no synthetic instruction trace.

The returned `RV32IInstructionRunResult.final_state` is a snapshot. For a limit stop, it includes the stop even though the last instruction trace describes the preceding attempt.

### Returned trace

| Field | Meaning |
| --- | --- |
| `instruction_index` | Attempt count before this call; zero-based for a fresh state |
| `pc_before`, `raw_instruction`, `decoded` | Instruction identity and decoded fields, when fetch succeeds |
| `rs1_value`, `rs2_value`, `immediate` | Old operand values and decoded immediate; use only operands meaningful for this instruction |
| `writeback` | Whether a destination write was requested, its register/value, and whether `x0` discarded it |
| `memory` | Access kind, address, width, load/store value, and fault flag; a faulting access is not a completed store |
| `pc_after`, `halted`, `trapped`, `trap_cause` | Next PC and execution outcome |

The trace also contains internal decoded controls and intermediate arithmetic values for diagnostics. They are not additional architectural state or requirements on the later core's internal wiring. No trace field specifies cycle count or propagation delay.

## Running the reference

This complete example starts at PC `0x40`, executes `add x5, x6, x7` with source values 10 and 3, and then executes EBREAK. It uses separate memories and requires no simulator setup.

<!-- rv32i-reference-usage -->
```cpp
#include "rv32i/RV32IInstructionOracle.hpp"
#include <iomanip>
#include <iostream>

int main() {
    rv32i::RV32IState state;
    rv32i::RV32IFunctionalMemory imem, dmem;
    state.pc = 0x40;
    state.writeRegister(6, 10);
    state.writeRegister(7, 3);
    imem.loadWords(0x40, {
        0x007302B3U,  // add x5, x6, x7
        0x00100073U,  // ebreak
    });

    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        const auto trace = rv32i::RV32IInstructionOracle::step(state, imem, dmem);
        std::cout << rv32i::toString(trace.decoded.instruction)
                  << " x5=" << std::dec << state.readRegister(5)
                  << " pc=0x" << std::hex << trace.pc_before
                  << "->0x" << trace.pc_after
                  << " halted=" << state.halted
                  << " trapped=" << state.trapped << '\n';
    }
    std::cout << "attempts=" << std::dec << state.instruction_count << '\n';
}
```

Expected output:

<!-- rv32i-reference-usage-output -->
```text
ADD x5=13 pc=0x40->0x44 halted=0 trapped=0
EBREAK x5=13 pc=0x44->0x44 halted=1 trapped=0
attempts=2
```

Save the example as `rv32i-reference-example.cpp`. From the repository root, after configuring the native build:

```sh
cmake --build build --target SimulatorLib
c++ -std=c++20 -Iinclude rv32i-reference-example.cpp build/libSimulatorLib.a -o build/rv32i-reference-example
./build/rv32i-reference-example
```

`RV32IReferenceDocumentationTest` compiles the exact example from this page against the built library and checks its output.

## Complete behavioral implementation

The listings are included directly from repository files when the site builds. They contain the instruction-level implementation and its state, decoding, control-selection, and functional-memory definitions. They are not a second implementation maintained only for documentation.

The source retains an additional simulator-integration overload; standalone callers use the `step()` and `run()` forms above. Normal repository headers and the built library supply the remaining dependencies. Clocked core adapters and their interfaces belong to the later core documentation.

??? example "State, instruction fields, and execution trace"

    <!-- simulator-source: include/rv32i/RV32IState.hpp -->

    <!-- simulator-source: include/rv32i/RV32IInstruction.hpp -->

    <!-- simulator-source: include/rv32i/RV32IInstructionTrace.hpp -->

??? example "Complete decoder and immediate extraction"

    <!-- simulator-source: include/rv32i/RV32IDecoder.hpp -->

    <!-- simulator-source: src/rv32i/RV32IDecoder.cpp -->

??? example "Complete decoded-control mapping"

    <!-- simulator-source: include/rv32i/RV32IControl.hpp -->

    <!-- simulator-source: src/rv32i/RV32IControl.cpp -->

??? example "Complete functional memory"

    <!-- simulator-source: include/rv32i/RV32IFunctionalMemory.hpp -->

    <!-- simulator-source: src/rv32i/RV32IFunctionalMemory.cpp -->

??? example "Complete instruction-step engine and bounded runner"

    <!-- simulator-source: include/rv32i/RV32IInstructionOracle.hpp -->

    <!-- simulator-source: src/rv32i/RV32IInstructionOracle.cpp -->

## Using it as an answer sheet

For a later core, initialize the same register/PC values and program/data images. After each completed instruction attempt, compare the architectural registers, next PC, successful memory effects, and halt/trap outcome with the reference. Compare at instruction boundaries, not at matching simulator timestamps.

The reference is a test oracle, not an infallible specification. Check discrepancies against the instruction rules and explicit expected examples. A behavioral adapter that calls this same oracle is not an independent implementation; agreement between the two alone does not validate the ISA.

[Programs and verification](programs/index.md) supplies full expected results and embedded runs. This page defines the executable behavior; it does not choose the datapath, control wiring, or timing of the core that will implement it.
