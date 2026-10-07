# § 4.1. RV32I Reference

The instruction-level specification and executable answer sheet for CircuitSim's RV32I core. The single-cycle and five-stage cores must agree on the ordered register, memory, PC, and terminal effects defined here; their internal circuits and timing may differ.

Hardware component interfaces and implementations are documented separately in [4.3. RV32I Building Blocks](../4.3/index.md).

## Contents

| Section | Contents |
| --- | --- |
| [4.1.1. Machine model and common conventions](machine.md) | State, numeric and memory rules, common exceptions, register names, and stack conventions |
| [4.1.2. Instruction formats and decoding](formats.md) | Field positions, immediate reconstruction, selection rules, and invalid encodings |
| [4.1.3. Instruction reference](instructions/index.md) | All 40 instructions, each with syntax, encoding, operands, operation, exceptions, and an example |
| [4.1.4. Executable RV32I reference](implementation.md) | Architectural state, instruction-step API, complete behavioral implementation, and a runnable example |

Programs and verification are documented in [4.2. Benchmarking and Verification](../4.2/index.md), including all 22 fixed programs, generated workloads, compiled workloads, and the shared measurement contract. The one-step instruction examples remain with the executable reference above.

## Instruction families

| Family | Instructions |
| --- | --- |
| [Arithmetic](instructions/arithmetic.md) | [ADD](instructions/arithmetic.md#add), [SUB](instructions/arithmetic.md#sub), [ADDI](instructions/arithmetic.md#addi) |
| [Logic](instructions/logic.md) | [AND](instructions/logic.md#and), [OR](instructions/logic.md#or), [XOR](instructions/logic.md#xor), [ANDI](instructions/logic.md#andi), [ORI](instructions/logic.md#ori), [XORI](instructions/logic.md#xori) |
| [Shifts](instructions/shifts.md) | [SLL](instructions/shifts.md#sll), [SRL](instructions/shifts.md#srl), [SRA](instructions/shifts.md#sra), [SLLI](instructions/shifts.md#slli), [SRLI](instructions/shifts.md#srli), [SRAI](instructions/shifts.md#srai) |
| [Comparisons](instructions/comparisons.md) | [SLT](instructions/comparisons.md#slt), [SLTU](instructions/comparisons.md#sltu), [SLTI](instructions/comparisons.md#slti), [SLTIU](instructions/comparisons.md#sltiu) |
| [Upper Immediates](instructions/upper-immediates.md) | [LUI](instructions/upper-immediates.md#lui), [AUIPC](instructions/upper-immediates.md#auipc) |
| [Loads](instructions/loads.md) | [LB](instructions/loads.md#lb), [LH](instructions/loads.md#lh), [LW](instructions/loads.md#lw), [LBU](instructions/loads.md#lbu), [LHU](instructions/loads.md#lhu) |
| [Stores](instructions/stores.md) | [SB](instructions/stores.md#sb), [SH](instructions/stores.md#sh), [SW](instructions/stores.md#sw) |
| [Branches](instructions/branches.md) | [BEQ](instructions/branches.md#beq), [BNE](instructions/branches.md#bne), [BLT](instructions/branches.md#blt), [BGE](instructions/branches.md#bge), [BLTU](instructions/branches.md#bltu), [BGEU](instructions/branches.md#bgeu) |
| [Jumps](instructions/jumps.md) | [JAL](instructions/jumps.md#jal), [JALR](instructions/jumps.md#jalr) |
| [Ordering and System](instructions/system.md) | [FENCE](instructions/system.md#fence), [ECALL](instructions/system.md#ecall), [EBREAK](instructions/system.md#ebreak) |

## Scope and authorities

The scope is the 40 instructions of **RV32I version 2.1**, with CircuitSim's explicitly documented execution-environment choices. Extensions and privileged machinery are outside this reference.

- [Ratified RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html): instruction encodings and architectural semantics.
- [RISC-V ABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/): software register and calling conventions, not additional hardware instructions.

The [instruction catalogue](instructions/index.md) is the primary lookup view. ISA rules, simulator policies, and software conventions are distinguished throughout.
