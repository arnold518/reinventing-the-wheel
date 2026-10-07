# § 4.1.4. Executable RV32I Reference

[Reference index](index.md) · [Instruction catalogue](instructions/index.md) · [Benchmarking and verification](../4.2/index.md)

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

[Fixed programs](../4.2/programs/index.md) supplies full expected results and embedded runs. This page defines the executable behavior; it does not choose the datapath, control wiring, or timing of the core that will implement it.

## One-step examples for all 40 instructions

Each row is an **independent** attempt at `P=0x00000040`, with `x6=A`, `x7=B`, every other register zero, and separate instruction/data memory. The instruction word is loaded at `P`. Initially `R32(0x100)=0x80FF7F01`, so bytes at `0x100`–`0x103` are `01 7F FF 80`; all other data bytes are zero.

The columns give the resulting `x5`, next PC, word at `0x100`, and terminal status. All arithmetic/register-destination examples use `rd=x5`; stores and branches have no destination. `A`/`B` remain unchanged. Values are hexadecimal words. `Run` means neither halted nor trapped; `Halt` means the project's EBREAK stop; `EnvironmentCall` is a trap. These explicit expectations are executed by `RV32IReferenceDocumentationTest`, not inferred by copying the oracle output.

<!-- rv32i-step-examples -->
| Instruction | Word | A | B | Expected x5 | Expected PC | Expected memory[0x100] | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ADD` | `0x007302B3` | `0x7FFFFFFF` | `0x00000001` | `0x80000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SUB` | `0x407302B3` | `0x00000000` | `0x00000001` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `SLL` | `0x007312B3` | `0x00000001` | `0x00000021` | `0x00000002` | `0x00000044` | `0x80FF7F01` | Run |
| `SLT` | `0x007322B3` | `0xFFFFFFFF` | `0x00000001` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTU` | `0x007332B3` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `XOR` | `0x007342B3` | `0x000000AA` | `0x0000000F` | `0x000000A5` | `0x00000044` | `0x80FF7F01` | Run |
| `SRL` | `0x007352B3` | `0x80000000` | `0x00000001` | `0x40000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SRA` | `0x407352B3` | `0x80000000` | `0x00000001` | `0xC0000000` | `0x00000044` | `0x80FF7F01` | Run |
| `OR` | `0x007362B3` | `0x000000A0` | `0x0000000F` | `0x000000AF` | `0x00000044` | `0x80FF7F01` | Run |
| `AND` | `0x007372B3` | `0x000000AA` | `0x0000000F` | `0x0000000A` | `0x00000044` | `0x80FF7F01` | Run |
| `ADDI` | `0xFFF30293` | `0x00000000` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTI` | `0x00032293` | `0xFFFFFFFF` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SLTIU` | `0xFFF33293` | `0xFFFFFFFE` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `XORI` | `0xFFF34293` | `0x12345678` | `0x00000000` | `0xEDCBA987` | `0x00000044` | `0x80FF7F01` | Run |
| `ORI` | `0x80036293` | `0x00000001` | `0x00000000` | `0xFFFFF801` | `0x00000044` | `0x80FF7F01` | Run |
| `ANDI` | `0x7FF37293` | `0x12345678` | `0x00000000` | `0x00000678` | `0x00000044` | `0x80FF7F01` | Run |
| `SLLI` | `0x01F31293` | `0x00000001` | `0x00000000` | `0x80000000` | `0x00000044` | `0x80FF7F01` | Run |
| `SRLI` | `0x01F35293` | `0x80000000` | `0x00000000` | `0x00000001` | `0x00000044` | `0x80FF7F01` | Run |
| `SRAI` | `0x41F35293` | `0x80000000` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `LB` | `0x00230283` | `0x00000100` | `0x00000000` | `0xFFFFFFFF` | `0x00000044` | `0x80FF7F01` | Run |
| `LH` | `0x00231283` | `0x00000100` | `0x00000000` | `0xFFFF80FF` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032283` | `0x00000100` | `0x00000000` | `0x80FF7F01` | `0x00000044` | `0x80FF7F01` | Run |
| `LBU` | `0x00234283` | `0x00000100` | `0x00000000` | `0x000000FF` | `0x00000044` | `0x80FF7F01` | Run |
| `LHU` | `0x00235283` | `0x00000100` | `0x00000000` | `0x000080FF` | `0x00000044` | `0x80FF7F01` | Run |
| `SB` | `0x007300A3` | `0x00000100` | `0x000000AB` | `0x00000000` | `0x00000044` | `0x80FFAB01` | Run |
| `SH` | `0x00731123` | `0x00000100` | `0x00001234` | `0x00000000` | `0x00000044` | `0x12347F01` | Run |
| `SW` | `0x00732023` | `0x00000100` | `0x12345678` | `0x00000000` | `0x00000044` | `0x12345678` | Run |
| `BEQ` | `0x00730463` | `0x00000005` | `0x00000005` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `BNE` | `0x00731463` | `0x00000005` | `0x00000005` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BLT` | `0x00734463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `BGE` | `0x00735463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BLTU` | `0x00736463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `BGEU` | `0x00737463` | `0xFFFFFFFF` | `0x00000001` | `0x00000000` | `0x00000048` | `0x80FF7F01` | Run |
| `JAL` | `0x008002EF` | `0x00000000` | `0x00000000` | `0x00000044` | `0x00000048` | `0x80FF7F01` | Run |
| `JALR` | `0x003302E7` | `0x00000046` | `0x00000000` | `0x00000044` | `0x00000048` | `0x80FF7F01` | Run |
| `LUI` | `0x123452B7` | `0x00000000` | `0x00000000` | `0x12345000` | `0x00000044` | `0x80FF7F01` | Run |
| `AUIPC` | `0x12345297` | `0x00000000` | `0x00000000` | `0x12345040` | `0x00000044` | `0x80FF7F01` | Run |
| `FENCE` | `0x0FF0000F` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `ECALL` | `0x00000073` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | EnvironmentCall |
| `EBREAK` | `0x00100073` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | Halt |

The table is vertically and horizontally scrollable. These one-step semantic examples have no simulator timestamps: `step()` itself is not clocked. Each program page provides its own timestamped checkpoint table.

### Boundary and exception answer sheet

These rows use the **same independent initial state** as the 40 examples above. Trap names replace `Run`/`Halt` in the status column. In particular, the last JALR row succeeds now; its next fetch would fail. A reserved-field FENCE is accepted, while an all-zero instruction is not.

<!-- rv32i-edge-examples -->
| Instruction | Word | A | B | Expected x5 | Expected PC | Expected memory[0x100] | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `JAL` | `0x002002EF` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `JALR` | `0x000302E7` | `0x00000046` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `BEQ` | `0x00730163` | `0x00000001` | `0x00000001` | `0x00000000` | `0x00000040` | `0x80FF7F01` | InstructionAddressMisaligned |
| `BNE` | `0x00731163` | `0x00000001` | `0x00000001` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032283` | `0x00000101` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAddressMisaligned |
| `LW` | `0x00032283` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `SW` | `0x00732023` | `0x00000102` | `0x11223344` | `0x00000000` | `0x00000040` | `0x80FF7F01` | StoreAddressMisaligned |
| `SW` | `0x00732023` | `0x00040000` | `0x11223344` | `0x00000000` | `0x00000040` | `0x80FF7F01` | StoreAccessFault |
| `LB` | `0x00030283` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `ADDI` | `0x00130013` | `0x00000005` | `0x00000000` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `LW` | `0x00032003` | `0x00040000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | LoadAccessFault |
| `JALR` | `0x000302E7` | `0x00040000` | `0x00000000` | `0x00000044` | `0x00040000` | `0x80FF7F01` | Run |
| `FENCE` | `0xFFF3028F` | `0x00000001` | `0x00000002` | `0x00000000` | `0x00000044` | `0x80FF7F01` | Run |
| `INVALID` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000000` | `0x00000040` | `0x80FF7F01` | IllegalInstruction |


## Additional arithmetic examples

These preserved examples use independent initial register values `x6=10`, `x7=3` and destination `x5`; PC starts at `0x40`. Numbers without `0x` are decimal. Each successful result advances PC to `0x44` and changes no memory. These examples are checked along with the per-instruction entries.

<details class="note" markdown="1">
<summary>Ten register examples and nine immediate examples</summary>

<!-- rv32i-register-reference -->
| Instruction | Value written to `rd` | Example and result |
| --- | --- | --- |
| `ADD rd, rs1, rs2` | First source plus second source | `add x5, x6, x7` → `x5=13` |
| `SUB rd, rs1, rs2` | First source minus second source | `sub x5, x6, x7` → `x5=7` |
| `SLL rd, rs1, rs2` | First source shifted left; zeros enter the low bits | `sll x5, x6, x7` → `x5=80` |
| `SLT rd, rs1, rs2` | `1` if first source is less than second, **signed**; otherwise `0` | `slt x5, x6, x7` → `x5=0` |
| `SLTU rd, rs1, rs2` | Same less-than test, **unsigned** | `sltu x5, x6, x7` → `x5=0` |
| `XOR rd, rs1, rs2` | Bitwise exclusive OR of the sources | `xor x5, x6, x7` → `x5=9` |
| `SRL rd, rs1, rs2` | First source shifted right; **zeros** enter the high bits | `srl x5, x6, x7` → `x5=1` |
| `SRA rd, rs1, rs2` | First source shifted right; **copies of its sign bit** enter the high bits | `sra x5, x6, x7` → `x5=1` |
| `OR rd, rs1, rs2` | Bitwise OR of the sources | `or x5, x6, x7` → `x5=11` |
| `AND rd, rs1, rs2` | Bitwise AND of the sources | `and x5, x6, x7` → `x5=2` |

<!-- rv32i-immediate-reference -->
| Instruction | Value written to `rd` | Example and result |
| --- | --- | --- |
| `ADDI rd, rs1, imm12` | Source plus immediate | `addi x5, x6, -1` → `x5=9` |
| `SLTI rd, rs1, imm12` | `1` if source is less than immediate, **signed**; otherwise `0` | `slti x5, x6, 11` → `x5=1` |
| `SLTIU rd, rs1, imm12` | Same less-than test, **unsigned after sign extension** | `sltiu x5, x6, -1` → `x5=1` |
| `XORI rd, rs1, imm12` | Bitwise exclusive OR with the extended immediate | `xori x5, x6, -1` → `x5=0xFFFFFFF5` |
| `ORI rd, rs1, imm12` | Bitwise OR with the extended immediate | `ori x5, x6, 5` → `x5=15` |
| `ANDI rd, rs1, imm12` | Bitwise AND with the extended immediate | `andi x5, x6, 7` → `x5=2` |
| `SLLI rd, rs1, shamt` | Source shifted left by `shamt`; zeros enter low bits | `slli x5, x6, 3` → `x5=80` |
| `SRLI rd, rs1, shamt` | Source shifted right by `shamt`; zeros enter high bits | `srli x5, x6, 1` → `x5=5` |
| `SRAI rd, rs1, shamt` | Source shifted right by `shamt`; sign-bit copies enter high bits | `srai x5, x6, 1` → `x5=5` |

</details>
