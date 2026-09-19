# Arithmetic

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[ADD](#add) · [SUB](#sub) · [ADDI](#addi)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## ADD — Add {#add}

### Purpose

Adds two register values and writes the low 32 bits of their sum.

### Assembly syntax

```asm
add rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00000033` |

Recognize when `(instruction & 0xFE00707F) == 0x00000033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `000` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation ADD -->
```python
write_rd(x[rs1] + x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Integer overflow wraps; it does not trap or set architectural carry/overflow flags.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
add x5, x6, x7
```

Instruction word: `0x007302B3`. The example is one independent instruction attempt.

<!-- rv32i-example ADD -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x80000000` |
| `x6` | `0x7FFFFFFF` | `0x7FFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SUB — Subtract {#sub}

### Purpose

Subtracts the second source register from the first.

### Assembly syntax

```asm
sub rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x40000033` |

Recognize when `(instruction & 0xFE00707F) == 0x40000033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0100000` | `rs2` | `rs1` | `000` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SUB -->
```python
write_rd(x[rs1] - x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Underflow wraps modulo 2^32. Operand order matters: this is rs1 minus rs2. The alias `neg rd, rs` expands to `sub rd, x0, rs`.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
sub x5, x6, x7
```

Instruction word: `0x407302B3`. The example is one independent instruction attempt.

<!-- rv32i-example SUB -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFFFFFF` |
| `x6` | `0x00000000` | `0x00000000` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## ADDI — Add Immediate {#addi}

### Purpose

Adds a signed 12-bit constant to a register value.

### Assembly syntax

```asm
addi rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00000013` |

Recognize when `(instruction & 0x0000707F) == 0x00000013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `000` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation ADDI -->
```python
write_rd(x[rs1] + imm)
pc = u32(pc + 4)
```

### Exceptions and special cases

The immediate is sign-extended. Overflow wraps without a trap. `mv rd, rs` and `nop` are ADDI aliases; there is no separate SUBI instruction.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
addi x5, x6, -1
```

Instruction word: `0xFFF30293`. The example is one independent instruction attempt.

<!-- rv32i-example ADDI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFFFFFF` |
| `x6` | `0x00000000` | `0x00000000` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
