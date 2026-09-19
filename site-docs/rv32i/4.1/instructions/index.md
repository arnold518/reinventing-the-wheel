# § 4.1.3. Instruction Reference

[Reference contents](../index.md) · [Machine conventions](../machine.md) · [Encoding rules](../formats.md)

Every entry uses the same sections: **Purpose · Assembly syntax · Encoding and decoding · Operands · Operation · Exceptions and special cases · Example**. Each instruction has a stable direct link; related instructions are not combined into a single definition.

## Instruction index

| Family | Instructions |
| --- | --- |
| [Arithmetic](arithmetic.md) | [ADD](arithmetic.md#add), [SUB](arithmetic.md#sub), [ADDI](arithmetic.md#addi) |
| [Logic](logic.md) | [AND](logic.md#and), [OR](logic.md#or), [XOR](logic.md#xor), [ANDI](logic.md#andi), [ORI](logic.md#ori), [XORI](logic.md#xori) |
| [Shifts](shifts.md) | [SLL](shifts.md#sll), [SRL](shifts.md#srl), [SRA](shifts.md#sra), [SLLI](shifts.md#slli), [SRLI](shifts.md#srli), [SRAI](shifts.md#srai) |
| [Comparisons](comparisons.md) | [SLT](comparisons.md#slt), [SLTU](comparisons.md#sltu), [SLTI](comparisons.md#slti), [SLTIU](comparisons.md#sltiu) |
| [Upper Immediates](upper-immediates.md) | [LUI](upper-immediates.md#lui), [AUIPC](upper-immediates.md#auipc) |
| [Loads](loads.md) | [LB](loads.md#lb), [LH](loads.md#lh), [LW](loads.md#lw), [LBU](loads.md#lbu), [LHU](loads.md#lhu) |
| [Stores](stores.md) | [SB](stores.md#sb), [SH](stores.md#sh), [SW](stores.md#sw) |
| [Branches](branches.md) | [BEQ](branches.md#beq), [BNE](branches.md#bne), [BLT](branches.md#blt), [BGE](branches.md#bge), [BLTU](branches.md#bltu), [BGEU](branches.md#bgeu) |
| [Jumps](jumps.md) | [JAL](jumps.md#jal), [JALR](jumps.md#jalr) |
| [Ordering and System](system.md) | [FENCE](system.md#fence), [ECALL](system.md#ecall), [EBREAK](system.md#ebreak) |

## Reading an entry

- Fixed values in bit-layout tables are **binary**. Hexadecimal words have a `0x` prefix.
- Register names in syntax are placeholders. `rd` is the destination; `rs1` and `rs2` are source register numbers.
- `offset(rs1)` means the old value in `rs1` plus a signed byte displacement.
- A branch/JAL label is encoded relative to the **current instruction**, not PC+4. `.+8` in an example denotes a label eight bytes after this instruction.
- Operation blocks use [defined Python-style helpers](../machine.md#notation-and-operation-helpers) and are tested against the compiled reference. They specify instruction effects, not event timing or separate clock cycles.
- The current instruction has already been fetched and legally decoded before its block starts. [Common fetch faults](../machine.md#common-exceptions) precede every instruction.

## Example conventions

Each example is a fresh, independent attempt. Its word is loaded at PC `0x40`; the machine is running with `halted=false` and `trapped=false`. All registers start at zero except the source values explicitly listed in that entry.

Data memory contains word `0x80FF7F01` at `0x100` (bytes `01 7F FF 80`), and is otherwise zero. Instruction/data memory are separate, each with the default 256 KiB capacity. Loads and stores repeat their relevant initial memory state in the entry. Other instructions leave data memory unchanged.

`Run` means execution can continue. `Halt` means CircuitSim's EBREAK convention. A named exception means a terminal trap. Every example increments the reference's attempted-instruction count once; this count is not a privileged retired-instruction counter.

The example word, operands, operation block, expected registers, PC, memory, and status are checked by `RV32IReferenceDocumentationTest`. [Program-level examples](../programs/index.md) additionally provide simulator times and instruction checkpoints.

## Assembler aliases

Aliases do not add decoder instructions. They expand into the following base instructions.

| Pseudoinstruction | RV32I expansion | Meaning |
| --- | --- | --- |
| `nop` | `addi x0, x0, 0` | Canonical NOP, word `0x00000013` |
| `mv rd, rs` | `addi rd, rs, 0` | Register copy |
| `not rd, rs` | `xori rd, rs, -1` | Bitwise complement |
| `neg rd, rs` | `sub rd, x0, rs` | Two's-complement negation |
| `seqz rd, rs` | `sltiu rd, rs, 1` | Set if zero |
| `snez rd, rs` | `sltu rd, x0, rs` | Set if nonzero |
| `j target` | `jal x0, target` | Direct jump without link |
| `jr rs` | `jalr x0, 0(rs)` | Indirect jump without link |
| `ret` | `jalr x0, 0(x1)` | Return through `ra` |
| `beqz rs, target` | `beq rs, x0, target` | Branch if zero |
| `bnez rs, target` | `bne rs, x0, target` | Branch if nonzero |
| `bgt rs1, rs2, target` | `blt rs2, rs1, target` | Signed greater-than |
| `ble rs1, rs2, target` | `bge rs2, rs1, target` | Signed less-than-or-equal |

`li`, `la`, and `call` may expand to multiple instructions depending on the value, relocation, and assembler/linker decisions. For a large address, LUI + JALR can construct an absolute target; AUIPC + JALR can construct a PC-relative target. The upper part must compensate when the lower 12-bit part is negative after sign extension.

Aliases such as `ret` do not perform stack loads themselves. Saving/restoring return addresses and allocating stack frames is software's responsibility; see [stack conventions](../machine.md#register-names-calls-and-the-stack).
