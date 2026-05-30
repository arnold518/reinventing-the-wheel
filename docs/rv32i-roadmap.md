# RV32I CPU Roadmap

This document defines the project path for turning CircuitSim into an RV32I CPU simulator that can run real assembly programs.

The goal is not "full RISC-V" yet. The first target is a correct, testable, educational RV32I machine:

- 32-bit integer registers.
- 32-bit program counter.
- RV32I base integer instructions.
- Little-endian instruction and data memory.
- Bare-metal assembly programs.
- Enough circuit integration to visualize the datapath, registers, ALU behavior, and control flow.

## Scope

### In Scope For The First CPU

- RV32I only.
- 32 architectural registers, `x0` through `x31`.
- `x0` hardwired to zero.
- 32-bit wrapping arithmetic.
- Byte-addressed memory.
- Little-endian loads, stores, and instruction fetch.
- Single-cycle CPU first.
- Raw binary program loading first.
- Assembly integration through an external RISC-V toolchain.
- Halt behavior through `EBREAK` or a project-specific stop condition.
- C++ tests for every instruction group.
- Visualizer scenario for the CPU once the CPU model is stable.

### Out Of Scope For The First CPU

- RV64.
- Multiplication and division (`M` extension).
- Compressed 16-bit instructions (`C` extension).
- Atomics (`A` extension).
- Floating point (`F`, `D`, `Q` extensions).
- Vector instructions (`V` extension).
- Privileged mode.
- Virtual memory.
- Operating system boot.
- Interrupt controllers and timers.
- ELF loading as the first executable format.
- Cycle-accurate caches and pipelines.

These can be added after RV32I correctness is solid.

## Current Starting Point

The current codebase is a good educational foundation, but it is not yet an RV32I CPU.

Already useful:

- Event-driven simulator with wire update and component evaluation events.
- Hierarchical component model.
- Native multi-bit `Pin<WIDTH>` and `Wire<WIDTH>` support.
- Width-erased pin and wire APIs for builders, tests, simulation history, and bindings.
- Basic logic gates.
- `DFlipFlop` and `ClockGenerator`.
- 8-bit arithmetic, logic, mux, shifter, comparator, zero-detect, and `ALU8` modules.
- Structural 32-bit `Mux32to1`, `Mux32to1_32bit`, `Adder32`, `AddSub32`, `Logic32`, `ZeroDetect32`, `Comparator32`, `Shifter32`, and `ALU32` modules.
- Per-component 32-bit tests plus `RV32IALU32Test` coverage for the structural ALU32 milestone.
- Structural memory slices and practical behavioral memory components: `RegisterFile32x32`, `BehavioralRegisterFile32x32`, `Memory4x32`, `Memory32x32`, and `BehavioralMemory64Kx32`.
- Python visualizer for hierarchical circuits and simulation history.

Missing:

- Program counter.
- Instruction/data memory integration.
- RV32I instruction decoder.
- Control unit.
- Load/store unit.
- Branch and jump unit.
- CPU top-level component.
- Program loader.
- Assembly build workflow.
- RV32I decoder, state, executor, program, and CPU-level correctness tests.

## Official References

Use these as the rule sources when implementation details are unclear:

- RISC-V Unprivileged ISA, RV32I chapter: `https://docs.riscv.org/reference/isa/unpriv/rv32.html`
- RISC-V Unprivileged ISA PDF: `https://docs.riscv.org/reference/isa/_attachments/riscv-unprivileged.pdf`
- Official opcode database: `https://github.com/riscv/riscv-opcodes`
- Official base integer opcode file: `https://raw.githubusercontent.com/riscv/riscv-opcodes/master/extensions/rv_i`
- Official RV32I-specific opcode file: `https://raw.githubusercontent.com/riscv/riscv-opcodes/master/extensions/rv32_i`
- RISC-V Assembly Programmer's Manual: `https://github.com/riscv-non-isa/riscv-asm-manual`
- GNU assembler RISC-V options: `https://sourceware.org/binutils/docs/as/RISC_002dV_002dOptions.html`
- GNU assembler RISC-V directives: `https://sourceware.org/binutils/docs/as/RISC_002dV_002dDirectives.html`
- RISC-V psABI: `https://riscv-non-isa.github.io/riscv-elf-psabi-doc/`
- RISC-V architecture tests: `https://github.com/riscv/riscv-arch-test`
- Spike reference simulator: `https://github.com/riscv-software-src/riscv-isa-sim`

## Design Strategy

Build lower-level structure before behavioral shortcuts.

The core project rule is structural-first abstraction:

1. Define the external contract from the RV32I spec.
2. Implement the smallest practical lower-level version first using gates, slices, or composite components.
3. Test that lower-level implementation directly.
4. Add a behavioral version only when the lower-level implementation is too large, too slow, or too visually noisy to use everywhere.
5. Prove the behavioral version is an abstraction of the lower-level version with equivalence tests.

Behavioral modules are allowed, but they are not the source of truth. They are compressed implementations of already-understood lower-level hardware. If a full lower-level version is too large, build and test a representative lower-level slice first, such as a 1-bit ALU cell, 4-bit ALU slice, one register cell, one 32-bit register, or a tiny memory array. Then test the full behavioral module against that slice's contract plus RV32I architectural expectations.

The standalone functional RV32I executor in this roadmap is a test oracle and assembly runner. It should help validate ISA semantics, fixtures, and expected final states. It should not become the product CPU component unless the corresponding lower-level component contracts and equivalence tests exist.

### Behavioral Abstraction Rule

A behavioral component is acceptable only when all of these are true:

- A lower-level component or representative lower-level slice exists first.
- The lower-level version has direct tests.
- The behavioral version has the same pins, timing contract, and visible state contract, unless a documented wrapper explains the difference.
- Tests compare behavioral outputs against lower-level outputs over exhaustive inputs when feasible.
- For large input spaces, tests combine edge cases, randomized cases, and ISA-level program tests.
- The component documentation states why behavioral abstraction is needed.
- The visualizer can still expose the lower-level version for learning when practical.

This keeps the project honest: we can use behavioral models for scale, but only as tested abstractions of hardware we have already built or reduced to smaller proven pieces.

## Milestone 0: Project Rules And Test Programs

Purpose: define the exact target so every later decision is consistent.

Deliverables:

- `docs/rv32i-roadmap.md`.
- A short `docs/rv32i-isa-notes.md` later, with project-specific interpretations.
- Initial sample programs under `programs/rv32i/`.
- Initial test fixtures under `tests/fixtures/rv32i/`.

Decisions:

- Target ISA string: `rv32i`.
- Target ABI for external tools: `ilp32`.
- Assembly flags: `-march=rv32i -mabi=ilp32 -mno-relax`.
- First program format: raw little-endian instruction binary.
- First halt convention: `EBREAK` stops the simulator.
- First memory model: flat RAM starting at address `0x00000000`.
- First reset PC: `0x00000000`.

Done when:

- The repo has a written RV32I scope.
- The first tiny assembly programs are listed.
- The assembler command is documented.

Suggested starter programs:

- `halt.S`: executes `ebreak`.
- `addi.S`: writes constants into registers.
- `loop.S`: counts down to zero.
- `branch.S`: tests taken and not-taken branches.
- `memory.S`: stores and loads bytes, halfwords, and words.
- `fib.S`: computes a small Fibonacci number.

## Milestone 1: Structural RV32I ALU32

Purpose: build the first RV32I hardware foundation as lower-level combinational logic before adding a product-facing behavioral CPU.

The RV32I ALU is the right first implementation milestone because most later CPU work depends on it:

- Integer register-register operations.
- Integer register-immediate operations.
- Load/store effective address calculation.
- `JALR` target calculation.
- `AUIPC` address calculation.
- Branch comparisons.
- `LUI` writeback pass-through.

### ALU32 Reference Rules

These rules come from the official RV32I unprivileged ISA and the official RISC-V opcode database:

- RV32I has `XLEN=32`; integer registers and `pc` are 32 bits.
- Integer computational instructions operate on `XLEN`-bit register values.
- Integer arithmetic does not raise arithmetic exceptions.
- Addition and subtraction ignore overflow and write the low 32 result bits.
- Immediate arithmetic uses sign-extended 12-bit immediates before they reach the ALU.
- `SLT` and `SLTI` compare signed 32-bit values.
- `SLTU` and `SLTIU` compare unsigned 32-bit values. The immediate for `SLTIU` is sign-extended first, then treated as unsigned.
- `AND`, `OR`, `XOR`, `ANDI`, `ORI`, and `XORI` are bitwise operations.
- `SLL`, `SRL`, and `SRA` use the low 5 bits of `rs2` as the shift amount.
- `SLLI`, `SRLI`, and `SRAI` encode the shift amount in the low 5 bits of the I-immediate field.
- `SRA` and `SRAI` copy the original sign bit into vacated upper bits.
- `LUI` writes the U-immediate value with the low 12 bits filled with zero; the ALU can implement this as `PASS_B` when the immediate generator supplies the U-immediate.
- `AUIPC` adds the U-immediate to the current instruction address; the ALU can implement this as `ADD`.
- Load/store effective addresses are `rs1 + sign_extended_imm12`; the ALU can implement this as `ADD`.
- `JALR` target base is `rs1 + sign_extended_imm12`; clearing target bit 0 belongs in next-PC logic, not inside the generic ALU result.
- Branches use signed or unsigned comparisons, but RISC-V has no architectural condition-code register. ALU flags are internal project signals only.

### ALU32 Non-Goals

The first ALU must not implement:

- RV64 operations.
- `M` extension multiply/divide.
- `C` extension compressed instruction behavior.
- Floating-point operations.
- Vector operations.
- CSR behavior.
- Memory byte/halfword sign extension.
- Instruction decoding.
- Register `x0` enforcement.
- Trap or exception policy.

Those belong to later CPU blocks.

### ALU32 External Contract

Recommended pins:

| Pin | Width | Direction | Purpose |
| --- | --- | --- | --- |
| `A` | 32 | input | First operand, usually `rs1` or `pc` |
| `B` | 32 | input | Second operand, immediate, shift amount carrier, or pass-through value |
| `OP` | 5 | input | ALU operation select |
| `OUT` | 32 | output | Selected 32-bit result |
| `ZERO` | 1 | output | High when `OUT == 0` |
| `EQ` | 1 | output | High when `A == B` |
| `LT_SIGNED` | 1 | output | High when signed `A < B` |
| `LT_UNSIGNED` | 1 | output | High when unsigned `A < B` |
| `NEGATIVE` | 1 | output | Copy of `OUT[31]` |
| `CARRY_OUT` | 1 | output | Carry out from add/sub datapath |
| `OVERFLOW` | 1 | output | Signed overflow from add/sub datapath |

Flag rules:

- `ZERO` is based on the selected `OUT`.
- `EQ`, `LT_SIGNED`, and `LT_UNSIGNED` are based on comparing `A` and `B`, independent of the selected `OP`.
- `CARRY_OUT` and `OVERFLOW` are useful for testing and comparison logic, but they are not architectural RISC-V state.
- For subtraction implemented as `A + ~B + 1`, unsigned less-than is `!CARRY_OUT`.
- Signed less-than for subtraction is `SUB_RESULT[31] xor OVERFLOW`.

### ALU32 Operation Codes

Keep ALU operation codes project-local. They do not need to equal RISC-V opcodes.

| ALU OP | Name | Result |
| --- | --- | --- |
| `0x00` | `ADD` | `A + B` |
| `0x01` | `SUB` | `A - B` |
| `0x02` | `AND` | `A & B` |
| `0x03` | `OR` | `A | B` |
| `0x04` | `XOR` | `A ^ B` |
| `0x05` | `SLL` | `A << B[4:0]` |
| `0x06` | `SRL` | logical `A >> B[4:0]` |
| `0x07` | `SRA` | arithmetic `A >> B[4:0]` |
| `0x08` | `SLT` | `1` if signed `A < B`, else `0` |
| `0x09` | `SLTU` | `1` if unsigned `A < B`, else `0` |
| `0x0A` | `PASS_A` | `A` |
| `0x0B` | `PASS_B` | `B` |
| `0x0C` | `ZERO` | `0` |

Reserve the remaining `OP` values for later extensions or debug helpers.

### RV32I Instruction To ALU Mapping

| RV32I instruction group | ALU usage |
| --- | --- |
| `ADD`, `ADDI` | `ADD` |
| `SUB` | `SUB` |
| `AND`, `ANDI` | `AND` |
| `OR`, `ORI` | `OR` |
| `XOR`, `XORI` | `XOR` |
| `SLL`, `SLLI` | `SLL`, using `B[4:0]` |
| `SRL`, `SRLI` | `SRL`, using `B[4:0]` |
| `SRA`, `SRAI` | `SRA`, using `B[4:0]` |
| `SLT`, `SLTI` | `SLT` |
| `SLTU`, `SLTIU` | `SLTU` |
| `BEQ` | branch unit uses `EQ` |
| `BNE` | branch unit uses `!EQ` |
| `BLT` | branch unit uses `LT_SIGNED` |
| `BGE` | branch unit uses `!LT_SIGNED` |
| `BLTU` | branch unit uses `LT_UNSIGNED` |
| `BGEU` | branch unit uses `!LT_UNSIGNED` |
| `LB`, `LH`, `LW`, `LBU`, `LHU` | `ADD` for effective address |
| `SB`, `SH`, `SW` | `ADD` for effective address |
| `LUI` | `PASS_B` with U-immediate on `B` |
| `AUIPC` | `ADD` with `A=pc`, `B=U-immediate` |
| `JAL` | usually separate PC adder; can use `ADD` for target if shared |
| `JALR` | `ADD` for target base; next-PC logic clears bit 0 |

### Structural Build Order

Recommended files:

- `include/modules/composite/Adder32.hpp`
- `include/modules/composite/AddSub32.hpp`
- `include/modules/composite/Logic32.hpp`
- `include/modules/composite/ZeroDetect32.hpp`
- `include/modules/composite/Comparator32.hpp`
- `include/modules/composite/Shifter32.hpp`
- `include/modules/composite/ALU32.hpp`
- Matching `.cpp` files.
- `src/tests/RV32IALU32Tests.cpp`.

Build order:

1. Verify or extend 1-bit `FullAdder` tests.
2. Build a 4-bit adder slice from full adders.
3. Build `Adder32` from slices or full adders.
4. Build `AddSub32` using `B xor SUB` and carry-in `SUB`.
5. Build bitwise `Logic32` lanes from existing gates.
6. Build `ZeroDetect32` as a reduction tree.
7. Build `Comparator32` from the subtract datapath:
   - `EQ = zero(A - B)`.
   - `LT_UNSIGNED = !sub_carry_out`.
   - `LT_SIGNED = sub_sign xor sub_overflow`.
8. Build `Shifter32` as a 5-stage barrel shifter:
   - stage 0 shifts by 1 when `B[0]`.
   - stage 1 shifts by 2 when `B[1]`.
   - stage 2 shifts by 4 when `B[2]`.
   - stage 3 shifts by 8 when `B[3]`.
   - stage 4 shifts by 16 when `B[4]`.
   - `SRA` fills vacated upper bits with original `A[31]`.
9. Build the `ALU32` result mux from the operation outputs.
10. Expose only the stable top-level pins listed above.

Structural-first rule:

- Do not add `ALU32Fast` or another behavioral full-width ALU until the structural `ALU32` has tests.
- If full structural simulation becomes too slow, keep the structural ALU as the correctness target and add a behavioral abstraction with equivalence tests.

### ALU32 Tests

Required tests:

- 1-bit full-adder exhaustive tests.
- 4-bit add/sub exhaustive tests.
- 32-bit add/sub edge cases:
  - `0 + 0`
  - `0xffffffff + 1`
  - `0x7fffffff + 1`
  - `0x80000000 - 1`
  - `0 - 1`
  - equal operands
- Bitwise logic cases:
  - all zeroes
  - all ones
  - alternating bit patterns
  - high-bit-only patterns
- Comparator edge cases:
  - signed negative vs positive.
  - `INT32_MIN` vs `0`.
  - `INT32_MAX` vs `INT32_MIN`.
  - equal values.
  - unsigned high-bit values.
- Shifter cases:
  - all shift amounts `0..31`.
  - `0x80000000`.
  - `0x7fffffff`.
  - `0xffffffff`.
  - `0x00000001`.
- ALU operation-select tests for every defined `OP`.
- Shift amount masking tests proving only `B[4:0]` is used.
- Later behavioral equivalence tests comparing `ALU32Fast` against structural `ALU32`.

Done when:

- Structural `ALU32` exists.
- Every defined `OP` is tested.
- RV32I edge cases above pass.
- Flags match the stated contract.
- No product-facing behavioral ALU exists without equivalence tests.

Status:

- Milestone 1 is implemented. Keep `docs/rv32i-milestone-1-report.md` updated as tests, performance, or design decisions change.

## Milestone 2: RV32I Decode Library

Purpose: create a small, tested decoder independent of the circuit simulator.

Recommended files:

- `include/isa/rv32i/Instruction.hpp`
- `include/isa/rv32i/Decoder.hpp`
- `src/isa/rv32i/Decoder.cpp`
- `tests/RV32IDecodeTest.cpp`

Instruction fields:

- `opcode`: bits `[6:0]`
- `rd`: bits `[11:7]`
- `funct3`: bits `[14:12]`
- `rs1`: bits `[19:15]`
- `rs2`: bits `[24:20]`
- `funct7`: bits `[31:25]`

Instruction formats:

- R-type: register-register arithmetic and logic.
- I-type: immediate arithmetic, loads, `JALR`, `FENCE`, `ECALL`, `EBREAK`.
- S-type: stores.
- B-type: conditional branches.
- U-type: `LUI`, `AUIPC`.
- J-type: `JAL`.

Base opcodes to support:

| Group | Opcode Bits | Instructions |
| --- | --- | --- |
| `LUI` | `0110111` | `LUI` |
| `AUIPC` | `0010111` | `AUIPC` |
| `JAL` | `1101111` | `JAL` |
| `JALR` | `1100111` | `JALR` |
| `BRANCH` | `1100011` | `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU` |
| `LOAD` | `0000011` | `LB`, `LH`, `LW`, `LBU`, `LHU` |
| `STORE` | `0100011` | `SB`, `SH`, `SW` |
| `OP-IMM` | `0010011` | `ADDI`, `SLTI`, `SLTIU`, `XORI`, `ORI`, `ANDI`, `SLLI`, `SRLI`, `SRAI` |
| `OP` | `0110011` | `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`, `OR`, `AND` |
| `MISC-MEM` | `0001111` | `FENCE` |
| `SYSTEM` | `1110011` | `ECALL`, `EBREAK` |

Implementation rules:

- Decode from a `uint32_t` instruction word.
- Preserve the raw instruction word for debugging.
- Produce an enum for the decoded operation.
- Produce immediate values already sign-extended to `int32_t`.
- Mark illegal encodings explicitly.
- Treat unsupported non-RV32I encodings as illegal.
- Keep decoding free of simulator state.

Tests:

- One test per instruction group.
- Known raw instruction words decoded into exact fields.
- Immediate sign-extension tests.
- Illegal encoding tests.
- Shift-immediate validation tests.

Done when:

- Every RV32I instruction decodes into a stable enum.
- All immediate formats are tested.
- Illegal instructions are distinguishable from valid instructions.

## Milestone 3: Functional Architectural State

Purpose: create the CPU state model needed to execute instructions correctly.

Recommended files:

- `include/isa/rv32i/RegisterFile.hpp`
- `include/isa/rv32i/Memory.hpp`
- `include/isa/rv32i/CoreState.hpp`
- `src/isa/rv32i/RegisterFile.cpp`
- `src/isa/rv32i/Memory.cpp`
- `tests/RV32IStateTest.cpp`

Register file rules:

- 32 registers.
- Register width is 32 bits.
- Reads from `x0` always return `0`.
- Writes to `x0` are ignored.
- Register names in debug output should include ABI aliases later, but the core should use register numbers.

Memory rules:

- Byte-addressed.
- Little-endian.
- Bounds-checked.
- Loads support signed and unsigned extension.
- Stores update only the addressed bytes.
- Instruction fetch reads a 32-bit little-endian word.

Misaligned access policy:

- Start strict: misaligned `LH`, `LW`, `SH`, and `SW` trap or return an execution error.
- Document this behavior in tests.
- Add optional permissive behavior only if needed later.

Core state:

- `pc`: current program counter.
- `regs`: register file.
- `memory`: flat memory.
- `halted`: true after `EBREAK`.
- `trap`: optional error state for illegal instruction, invalid memory, or misalignment.
- `cycle_count`: increments once per executed instruction in the functional model.

Done when:

- Register zero behavior is tested.
- Memory endian behavior is tested.
- Signed and unsigned load behavior is tested.
- Misalignment behavior is tested.

## Milestone 4: Functional RV32I Reference Executor

Purpose: provide an ISA reference oracle and assembly runner for tests.

This is not the product CPU architecture. It is a compact way to compute expected results, validate program fixtures, and compare lower-level CircuitSim modules against RV32I semantics.

Recommended files:

- `include/isa/rv32i/Executor.hpp`
- `src/isa/rv32i/Executor.cpp`
- `tests/RV32IExecutorTest.cpp`

Execution API:

```cpp
struct StepResult {
    bool halted;
    bool trapped;
    std::string trap_reason;
    uint32_t old_pc;
    uint32_t new_pc;
    uint32_t instruction_word;
};

StepResult step(CoreState& state);
```

Execution rules:

- Fetch instruction at `pc`.
- Decode instruction.
- Execute instruction.
- Write back register result when needed.
- Update `pc`.
- Keep `x0` hardwired to zero after every step.
- Increment `cycle_count`.
- Stop on `EBREAK`.
- Report illegal instructions as traps.

Instruction behavior:

- `LUI`: write upper immediate.
- `AUIPC`: write `pc + upper immediate`.
- `JAL`: write link address and jump to `pc + offset`.
- `JALR`: write link address and jump to `(rs1 + imm) & ~1`.
- Branches: compare registers and either jump to `pc + offset` or continue to `pc + 4`.
- Loads: read memory and sign/zero extend.
- Stores: write byte, halfword, or word.
- Integer immediate ops: compute with sign-extended immediate.
- Integer register ops: compute with `rs1` and `rs2`.
- `FENCE`: no-op for the first single-core model.
- `ECALL`: trap or halt by project policy.
- `EBREAK`: halt.

Tests:

- One direct unit test per instruction.
- Program-level tests for loops and memory.
- Signed comparison tests.
- Unsigned comparison tests.
- Shift edge cases.
- Jump target alignment tests.
- `x0` write protection tests.

Done when:

- A raw binary containing RV32I instructions can run to `EBREAK`.
- Each instruction has at least one focused test.
- Common edge cases have explicit tests.
- The executor is documented as reference-only, not as a replacement for lower-level CPU modules.

## Milestone 5: Program Loader And Assembly Workflow

Purpose: let the project run real `.S` assembly files instead of hand-written machine words.

Recommended files:

- `tools/build-rv32i-program.sh`
- `tools/rv32i-bin-to-fixture.py` if useful.
- `programs/rv32i/*.S`
- `tests/RV32IProgramTest.cpp`

External toolchain assumptions:

- Prefer `riscv32-unknown-elf-*` if installed.
- Also allow `riscv64-unknown-elf-*` with `-march=rv32i -mabi=ilp32`.
- Use `objcopy -O binary` to produce raw binaries.

Suggested build flow:

```bash
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 -mno-relax -o program.o program.S
riscv64-unknown-elf-ld -Ttext=0x0 -o program.elf program.o
riscv64-unknown-elf-objcopy -O binary program.elf program.bin
```

Testing approach:

- Keep committed binary fixtures small.
- Keep source assembly files committed.
- If the toolchain is unavailable, tests should still run from committed fixtures.
- If the toolchain is available, add an optional test that rebuilds fixtures and compares bytes.

Done when:

- A committed assembly program can be assembled into a raw binary.
- The functional core can load and run that binary.
- Tests can run without requiring a local RISC-V toolchain.

## Milestone 6: CPU Component Contract And Abstraction Boundary

Purpose: make the RV32I CPU available inside CircuitSim without letting a behavioral shortcut become the source of truth.

Recommended files:

- `include/modules/riscv/RV32ICpu.hpp`
- `src/modules/riscv/RV32ICpu.cpp`
- `tests/RV32ICpuComponentTest.cpp`

Initial component interface:

| Pin | Width | Direction | Purpose |
| --- | --- | --- | --- |
| `CLK` | 1 | input | Execute one instruction on rising edge or scheduled clock tick |
| `RESET` | 1 | input | Reset PC, registers, and halted state |
| `HALTED` | 1 | output | High after `EBREAK` |
| `TRAPPED` | 1 | output | High after illegal instruction or memory error |
| `PC` | 32 | output | Current program counter for visualization |
| `INSTR` | 32 | output | Last fetched instruction |

Component behavior:

- On reset, initialize state.
- On clock event, execute one instruction if not halted or trapped.
- Update output pins after each step.
- Keep memory internal at first.
- Add explicit methods for test loading, such as `loadProgram(std::vector<uint8_t>)`.

Important integration choice:

- This component may start as a thin integration wrapper around lower-level blocks.
- A temporary reference-only behavioral CPU may exist for program bring-up, but it must be clearly named and tested as a reference model.
- A product-facing behavioral CPU component is allowed only after lower-level block contracts and equivalence tests exist.
- It should use the existing simulator timing model.
- It should not attempt to expose every internal CPU signal yet.

Done when:

- A CircuitSim test can instantiate `RV32ICpu`.
- The test can load a tiny program.
- The simulator can clock the CPU until halt.
- `PC`, `INSTR`, `HALTED`, and `TRAPPED` outputs are observable.
- The implementation path is documented as lower-level, behavioral abstraction, or reference-only.
- Any product-facing behavioral CPU path has equivalence tests against lower-level blocks. The reference executor may provide expected final program state as an additional check.

## Milestone 7: Datapath Integration And ALU32 Abstraction

Purpose: connect the structural ALU32 to the rest of the CPU datapath and add a behavioral abstraction only if the structural path is too slow for broad program runs.

Recommended files:

- `include/modules/composite/ImmediateGenerator32.hpp`
- `include/modules/composite/BranchDecision32.hpp`
- `include/modules/composite/NextPc32.hpp`
- `include/modules/composite/ALU32Fast.hpp` only if needed.
- Matching `.cpp` files and tests.

Recommended strategy:

- Keep the structural `ALU32` from Milestone 1 as the correctness target.
- Add immediate generation before trying to execute I-type, S-type, B-type, U-type, and J-type instructions.
- Add branch-decision logic that consumes `EQ`, `LT_SIGNED`, and `LT_UNSIGNED`.
- Add next-PC logic for `pc + 4`, branch targets, `JAL`, and `JALR`.
- Keep `JALR` bit-0 clearing outside the generic ALU result.
- Add `ALU32Fast` only if tests or visualizer scenarios become too slow.
- If `ALU32Fast` exists, give it the same external contract as structural `ALU32`.

Done when:

- Immediate generation produces the values expected by the decoder tests.
- Branch-decision logic maps RV32I branch instructions to the correct flag checks.
- Next-PC logic handles sequential, branch, `JAL`, and `JALR` paths.
- `ALU32Fast`, if present, passes equivalence tests against structural `ALU32`.
- The functional reference executor and datapath blocks agree on ALU, branch, and target-address edge cases.

## Milestone 8: Register File, PC, And Memories As Components

Purpose: split the behavioral CPU into understandable hardware blocks.

Recommended files:

- `include/modules/riscv/RegisterFile32.hpp`
- `include/modules/riscv/ProgramCounter32.hpp`
- `include/modules/riscv/InstructionMemory.hpp`
- `include/modules/riscv/DataMemory.hpp`
- Matching `.cpp` files and tests.

Register file interface:

| Pin | Width | Direction | Purpose |
| --- | --- | --- | --- |
| `CLK` | 1 | input | Commit write on clock edge |
| `WE` | 1 | input | Write enable |
| `RS1` | 5 | input | Read register 1 |
| `RS2` | 5 | input | Read register 2 |
| `RD` | 5 | input | Write register |
| `WDATA` | 32 | input | Write data |
| `RDATA1` | 32 | output | Read data 1 |
| `RDATA2` | 32 | output | Read data 2 |

Program counter interface:

| Pin | Width | Direction | Purpose |
| --- | --- | --- | --- |
| `CLK` | 1 | input | Commit next PC |
| `RESET` | 1 | input | Reset to boot address |
| `NEXT_PC` | 32 | input | Next PC value |
| `PC` | 32 | output | Current PC |

Memory policy:

- Instruction memory can be read-only after load.
- Data memory can be behavioral.
- Add byte-enable signals for stores.
- Add signed/unsigned extension outside memory or in a small load/store unit.

Done when:

- Each block has standalone tests.
- The blocks can reproduce a simple fetch/decode/execute/writeback cycle.
- Register file still enforces `x0 = 0`.

## Milestone 9: RV32I Control Unit

Purpose: decode instructions into datapath control signals.

Recommended files:

- `include/modules/riscv/RV32IControl.hpp`
- `src/modules/riscv/RV32IControl.cpp`
- `tests/RV32IControlTest.cpp`

Inputs:

- `INSTR<32>`
- Optional ALU comparison flags.

Outputs:

- ALU operation select.
- Register write enable.
- Memory read enable.
- Memory write enable.
- Load size.
- Load signed/unsigned.
- Store size.
- ALU operand A select.
- ALU operand B select.
- Writeback source select.
- Branch condition select.
- Jump control.
- Halt/trap signal.

Implementation strategy:

- Start with a clear truth-table contract for each decoded RV32I operation.
- Use the decoder enum from Milestone 2.
- Prefer a visible ROM/PLA-style control component for the product CPU.
- A temporary behavioral control table may be used for reference tests, but it must match the documented control truth table.
- Keep control signal names stable so the visualizer can label them.

Done when:

- Every RV32I instruction maps to expected control signals.
- Illegal instructions raise trap control.
- The control unit tests match the functional executor behavior.

## Milestone 10: Single-Cycle RV32I Datapath

Purpose: build the first visible CPU from components.

Recommended files:

- `include/modules/riscv/RV32ISingleCycleCpu.hpp`
- `src/modules/riscv/RV32ISingleCycleCpu.cpp`
- `tests/RV32ISingleCycleCpuTest.cpp`

Top-level blocks:

- Program counter.
- Instruction memory.
- Instruction decoder/control unit.
- Register file.
- Immediate generator.
- ALU.
- Branch comparator.
- Data memory.
- Writeback mux.
- Next-PC mux.

Cycle behavior:

- At the start of a cycle, `PC` selects the instruction.
- Decoder produces control signals.
- Register file reads source registers.
- Immediate generator creates the immediate.
- ALU computes result or target data.
- Branch/jump logic selects `NEXT_PC`.
- Memory handles load/store.
- Writeback data is selected.
- On clock edge, register file and PC update.

Done when:

- The component CPU runs the same programs as the functional core.
- The test suite compares final register and memory state against the functional core.
- The visual hierarchy exposes the main blocks.

## Milestone 11: Visualizer Integration

Purpose: make the CPU understandable in the Pygame visualizer.

Recommended files:

- Updates to `visualizer/main.py`.
- Updates to `visualizer/layout.json` if committed layouts are useful.
- Optional CPU-specific visual helpers in `visualizer/`.

Visualizer scenarios:

- `rv32i_halt`
- `rv32i_addi`
- `rv32i_loop`
- `rv32i_memory`
- `rv32i_fib`

Display priorities:

- Current cycle.
- Current `PC`.
- Current instruction word.
- Decoded instruction name.
- Register file summary.
- Main control signals.
- ALU input and output values.
- Memory address/data for load/store.
- Halt/trap state.

Rendering approach:

- Keep the top-level CPU readable at normal zoom.
- Show internals only when zooming into CPU blocks.
- Draw 32-bit buses as thick lines with value labels.
- Avoid expanding all 32 lanes by default.

Done when:

- A user can run the visualizer and select an RV32I scenario.
- The CPU can be stepped or scrubbed through time.
- The important datapath values are readable without inspecting C++ logs.

## Milestone 12: Validation Against External References

Purpose: avoid accidentally creating a CPU that only passes our own assumptions.

Validation levels:

- Unit tests for decoder, memory, register file, ALU, and executor.
- Program tests for small assembly files.
- Differential tests against Spike or another known-good RV32I simulator when available.
- Selected RISC-V architecture tests.

Suggested differential flow:

1. Run a small program on the CircuitSim functional core.
2. Run the same program on Spike or another reference.
3. Compare final registers and memory.
4. Add the case as a regression test.

Done when:

- The core passes the project RV32I test programs.
- At least one external reference check is documented.
- Failing cases produce useful debug output.

## Milestone 13: Documentation And User Workflow

Purpose: make RV32I development repeatable.

Recommended docs:

- `docs/rv32i-roadmap.md`
- `docs/rv32i-isa-notes.md`
- `docs/rv32i-assembly-workflow.md`
- `docs/rv32i-debugging.md`

User workflows to document:

- Build the simulator.
- Assemble a program.
- Convert it to a binary.
- Load it into the CPU test.
- Run the CPU in C++ tests.
- Run the CPU in the visualizer.
- Interpret halt/trap states.
- Inspect register and memory output.

Done when:

- A new contributor can run `fib.S` without reading source code.
- The visualizer RV32I scenario has a short usage guide.
- Common mistakes are documented.

## Recommended Implementation Order

Follow this order unless there is a strong reason to change it:

1. Structural RV32I `ALU32`: add/sub, logic, compare, shift, flags.
2. Decode library.
3. Lower-level register primitives: register cell, register word, small register file.
4. Memory contract and tiny lower-level memory slice, plus the behavioral memory abstraction when justified.
5. Functional executor as a reference oracle and assembly runner.
6. Program loader.
7. Program-level tests.
8. Datapath integration: immediate generator, branch decision, next-PC logic.
9. 32-bit behavioral abstractions only where lower-level equivalents or slices already exist.
10. Register file, PC, and memory components.
11. Control unit.
12. Single-cycle CPU component.
13. Visualizer scenario.
14. External validation.

This sequence keeps the CPU grounded in lower-level hardware before adding scale-oriented abstractions.

## Testing Plan

Every milestone should add tests before it is considered complete.

Required test categories:

- Decoder tests.
- Immediate generation tests.
- Lower-level component tests.
- Lower-level to behavioral equivalence tests.
- Register file tests.
- Memory endian tests.
- Load/store sign-extension tests.
- ALU operation tests.
- Branch and jump tests.
- Program-level execution tests.
- Component-level clocked CPU tests.
- Visualizer import smoke test after bindings are added.

Important edge cases:

- Writes to `x0`.
- Negative immediates.
- Signed overflow wrapping.
- Signed vs unsigned comparisons.
- Shift amount masking.
- Branch target calculation.
- `JALR` target bit 0 clearing.
- Load byte and load halfword sign extension.
- Store byte and store halfword partial writes.
- Misaligned loads/stores.
- Illegal instructions.

## Acceptance Criteria For "RV32I Works"

The project can claim first-pass RV32I support when:

- All RV32I base instructions are decoded.
- All RV32I base instructions execute.
- Lower-level versions or representative slices exist for the main CPU blocks.
- Behavioral abstractions have tests proving they match the lower-level contracts.
- `x0` behavior is correct.
- Loads and stores are little-endian and byte-addressed.
- Branches and jumps update `pc` correctly.
- `EBREAK` halts execution.
- Illegal instructions trap.
- Raw binary programs can be loaded.
- At least five assembly programs run to completion.
- C++ tests pass through CTest.
- The visualizer can show at least one RV32I CPU scenario.

## Later Extensions After RV32I

Do not start these until the first RV32I target is working:

- `M`: multiply and divide.
- `C`: compressed 16-bit instructions.
- `Zicsr`: control and status register instructions.
- Privileged machine mode.
- Interrupts and timers.
- ELF loader.
- Pipeline.
- Hazard detection and forwarding.
- Branch prediction.
- Caches.

The first extension should probably be `M`, because it is common and much smaller than compressed instruction support or privileged mode.

## Risks

### Risk: Building Too Much Gate-Level Logic Too Early

Mitigation:

- Keep the functional core as the correctness reference.
- Build small lower-level slices first.
- Use behavioral blocks only after lower-level contracts are tested.
- Scale from slice-level proof to full-width abstraction.

### Risk: Behavioral Shortcuts Become The Source Of Truth

Mitigation:

- Label temporary reference-only behavioral code clearly.
- Require lower-level or representative-slice tests before product-facing behavioral modules.
- Add equivalence tests whenever behavioral modules are introduced.

### Risk: Ambiguous RISC-V Behavior

Mitigation:

- Write down the project policy in `docs/rv32i-isa-notes.md`.
- Prefer official RISC-V documents.
- Add tests for every policy decision.

### Risk: Visualizer Clutter

Mitigation:

- Use collapsed 32-bit buses by default.
- Show block-level CPU first.
- Expand internals only on zoom or selection.

### Risk: Toolchain Availability

Mitigation:

- Commit small binary fixtures.
- Keep assembly sources next to fixtures.
- Make toolchain rebuilds optional.

## Near-Term Next Steps

Milestone 1 is complete:

- Structural `Mux32to1`, `Mux32to1_32bit`, `Adder32`, `AddSub32`, `Logic32`, `ZeroDetect32`, `Comparator32`, `Shifter32`, and `ALU32` exist.
- The ALU stack is wired into CMake, bindings, and CTest.
- `Mux32to1Test`, `Mux32to1_32bitTest`, `Adder32Test`, `AddSub32Test`, `Logic32Test`, `ZeroDetect32Test`, `Comparator32Test`, `Shifter32Test`, `ALU32Test`, and `RV32IALU32Test` cover the ALU stack and expose visualizer scenarios.
- Shift tests cover all shift amounts `0..31` and shift amount masking.

Next:

1. Add the RV32I decoder library.
2. Add decoder tests for every RV32I instruction group.
3. Move to register primitives, memory contracts, and the reference executor after the decoder is stable.
