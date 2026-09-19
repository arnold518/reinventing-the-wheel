# Shifts

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[SLL](#sll) · [SRL](#srl) · [SRA](#sra) · [SLLI](#slli) · [SRLI](#srli) · [SRAI](#srai)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## SLL — Shift Left Logical {#sll}

### Purpose

Shifts the first source left by the count supplied by the second source.

### Assembly syntax

```asm
sll rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00001033` |

Recognize when `(instruction & 0xFE00707F) == 0x00001033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `001` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SLL -->
```python
count = x[rs2] & 31
write_rd(x[rs1] << count)
pc = u32(pc + 4)
```

### Exceptions and special cases

Only the low five count bits are used. Counts 0 and 32 both select no shift; 33 selects a shift by 1. Zeros enter the low bits and shifted-out high bits are discarded.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
sll x5, x6, x7
```

Instruction word: `0x007312B3`. The example is one independent instruction attempt.

<!-- rv32i-example SLL -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000002` |
| `x6` | `0x00000001` | `0x00000001` |
| `x7` | `0x00000021` | `0x00000021` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SRL — Shift Right Logical {#srl}

### Purpose

Shifts the first source right, inserting zeros at the high end.

### Assembly syntax

```asm
srl rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00005033` |

Recognize when `(instruction & 0xFE00707F) == 0x00005033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `101` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SRL -->
```python
count = x[rs2] & 31
write_rd(x[rs1] >> count)
pc = u32(pc + 4)
```

### Exceptions and special cases

Only the low five count bits are used. The register value is treated as unsigned; its old sign bit is not replicated. Compare [SRA](#sra).

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
srl x5, x6, x7
```

Instruction word: `0x007352B3`. The example is one independent instruction attempt.

<!-- rv32i-example SRL -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x40000000` |
| `x6` | `0x80000000` | `0x80000000` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SRA — Shift Right Arithmetic {#sra}

### Purpose

Shifts the first source right, replicating its original sign bit.

### Assembly syntax

```asm
sra rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x40005033` |

Recognize when `(instruction & 0xFE00707F) == 0x40005033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0100000` | `rs2` | `rs1` | `101` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SRA -->
```python
count = x[rs2] & 31
write_rd(s32(x[rs1]) >> count)
pc = u32(pc + 4)
```

### Exceptions and special cases

Only the low five count bits are used. Negative values remain negative. This rounds negative values toward negative infinity, not toward zero. Compare [SRL](#srl).

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
sra x5, x6, x7
```

Instruction word: `0x407352B3`. The example is one independent instruction attempt.

<!-- rv32i-example SRA -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xC0000000` |
| `x6` | `0x80000000` | `0x80000000` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SLLI — Shift Left Logical Immediate {#slli}

### Purpose

Shifts a register left by a five-bit immediate count.

### Assembly syntax

```asm
slli rd, rs1, shamt
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0xFE00707F` |
| Match | `0x00001013` |

Recognize when `(instruction & 0xFE00707F) == 0x00001013`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `shamt` | `rs1` | `001` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `shamt` | Unsigned immediate shift count, 0–31. |

### Operation

<!-- rv32i-operation SLLI -->
```python
write_rd(x[rs1] << shamt)
pc = u32(pc + 4)
```

### Exceptions and special cases

The immediate count must be 0–31. Zeros enter the low bits; shifted-out high bits are discarded. Bits 31:25 must all be zero; a count of 32 is not encoded as a valid SLLI.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
slli x5, x6, 31
```

Instruction word: `0x01F31293`. The example is one independent instruction attempt.

<!-- rv32i-example SLLI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x80000000` |
| `x6` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SRLI — Shift Right Logical Immediate {#srli}

### Purpose

Shifts a register right by a five-bit immediate count, inserting zeros.

### Assembly syntax

```asm
srli rd, rs1, shamt
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0xFE00707F` |
| Match | `0x00005013` |

Recognize when `(instruction & 0xFE00707F) == 0x00005013`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `shamt` | `rs1` | `101` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `shamt` | Unsigned immediate shift count, 0–31. |

### Operation

<!-- rv32i-operation SRLI -->
```python
write_rd(x[rs1] >> shamt)
pc = u32(pc + 4)
```

### Exceptions and special cases

The immediate count must be 0–31 and bits 31:25 must all be zero. Unlike [SRAI](#srai), the original sign bit is not replicated.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
srli x5, x6, 31
```

Instruction word: `0x01F35293`. The example is one independent instruction attempt.

<!-- rv32i-example SRLI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000001` |
| `x6` | `0x80000000` | `0x80000000` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SRAI — Shift Right Arithmetic Immediate {#srai}

### Purpose

Shifts a register right by a five-bit immediate count, replicating its sign bit.

### Assembly syntax

```asm
srai rd, rs1, shamt
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0xFE00707F` |
| Match | `0x40005013` |

Recognize when `(instruction & 0xFE00707F) == 0x40005013`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0100000` | `shamt` | `rs1` | `101` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `shamt` | Unsigned immediate shift count, 0–31. |

### Operation

<!-- rv32i-operation SRAI -->
```python
write_rd(s32(x[rs1]) >> shamt)
pc = u32(pc + 4)
```

### Exceptions and special cases

Bits 31:25 must equal 0100000. The raw 12-bit field is 0x400 + shamt, but only shamt is the shift count. The immediate count must be 0–31. Compare [SRLI](#srli).

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
srai x5, x6, 31
```

Instruction word: `0x41F35293`. The example is one independent instruction attempt.

<!-- rv32i-example SRAI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFFFFFF` |
| `x6` | `0x80000000` | `0x80000000` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
