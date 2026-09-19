# Upper Immediates

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[LUI](#lui) · [AUIPC](#auipc)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## LUI — Load Upper Immediate {#lui}

### Purpose

Places a 20-bit immediate in the upper 20 bits of a register, with zeros in the bottom 12 bits.

### Assembly syntax

```asm
lui rd, imm20
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | U |
| Mask | `0x0000007F` |
| Match | `0x00000037` |

Recognize when `(instruction & 0x0000007F) == 0x00000037`. Fixed field values below are binary.

| `31:12` | `11:7` | `6:0` |
| --- | --- | --- |
| `imm[31:12]` | `rd` | `0110111` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `imm20` | 20-bit upper field, 0x00000–0xFFFFF. Decoded imm already has twelve low zero bits. |

### Operation

<!-- rv32i-operation LUI -->
```python
write_rd(imm)
pc = u32(pc + 4)
```

### Exceptions and special cases

This reads no memory and no source register. The assembly operand is the unshifted 20-bit field; decoded imm already includes the 12 low zeros. Do not shift it a second time.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
lui x5, 0x12345
```

Instruction word: `0x123452B7`. The example is one independent instruction attempt.

<!-- rv32i-example LUI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x12345000` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## AUIPC — Add Upper Immediate to PC {#auipc}

### Purpose

Adds an upper immediate to the address of this instruction and writes the result to a register.

### Assembly syntax

```asm
auipc rd, imm20
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | U |
| Mask | `0x0000007F` |
| Match | `0x00000017` |

Recognize when `(instruction & 0x0000007F) == 0x00000017`. Fixed field values below are binary.

| `31:12` | `11:7` | `6:0` |
| --- | --- | --- |
| `imm[31:12]` | `rd` | `0010111` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `imm20` | 20-bit upper field, 0x00000–0xFFFFF. Decoded imm already has twelve low zero bits. |

### Operation

<!-- rv32i-operation AUIPC -->
```python
write_rd(pc + imm)
pc = u32(pc + 4)
```

### Exceptions and special cases

Uses the old PC, not PC+4. This writes a register; it does not jump. Decoded imm is already shifted left by 12. `auipc rd, 0` copies the current instruction address.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
auipc x5, 0x12345
```

Instruction word: `0x12345297`. The example is one independent instruction attempt.

<!-- rv32i-example AUIPC -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x12345040` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
