# Comparisons

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[SLT](#slt) · [SLTU](#sltu) · [SLTI](#slti) · [SLTIU](#sltiu)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## SLT — Set Less Than Signed {#slt}

### Purpose

Writes 1 when the first register is less than the second in signed two's-complement order; otherwise writes 0.

### Assembly syntax

```asm
slt rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00002033` |

Recognize when `(instruction & 0xFE00707F) == 0x00002033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `010` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SLT -->
```python
write_rd(1 if s32(x[rs1]) < s32(x[rs2]) else 0)
pc = u32(pc + 4)
```

### Exceptions and special cases

The result is a full 32-bit word, 0 or 1; it is not a condition-code flag. For 0xFFFFFFFF and 1, SLT writes 1 because -1 < 1. Compare [SLTU](#sltu).

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
slt x5, x6, x7
```

Instruction word: `0x007322B3`. The example is one independent instruction attempt.

<!-- rv32i-example SLT -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000001` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SLTU — Set Less Than Unsigned {#sltu}

### Purpose

Writes 1 when the first register is less than the second in unsigned order; otherwise writes 0.

### Assembly syntax

```asm
sltu rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00003033` |

Recognize when `(instruction & 0xFE00707F) == 0x00003033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `011` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation SLTU -->
```python
write_rd(1 if x[rs1] < x[rs2] else 0)
pc = u32(pc + 4)
```

### Exceptions and special cases

For 0xFFFFFFFF and 1, SLTU writes 0: 4,294,967,295 is greater than 1. Compare [SLT](#slt). `snez rd, rs` expands to `sltu rd, x0, rs`.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
sltu x5, x6, x7
```

Instruction word: `0x007332B3`. The example is one independent instruction attempt.

<!-- rv32i-example SLTU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000000` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SLTI — Set Less Than Immediate Signed {#slti}

### Purpose

Compares a signed register value with a signed 12-bit constant and writes 1 or 0.

### Assembly syntax

```asm
slti rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00002013` |

Recognize when `(instruction & 0x0000707F) == 0x00002013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `010` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation SLTI -->
```python
write_rd(1 if s32(x[rs1]) < imm else 0)
pc = u32(pc + 4)
```

### Exceptions and special cases

Both operands are interpreted as signed values. The output is a 32-bit 0 or 1, and no architectural flags are changed.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
slti x5, x6, 0
```

Instruction word: `0x00032293`. The example is one independent instruction attempt.

<!-- rv32i-example SLTI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000001` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SLTIU — Set Less Than Immediate Unsigned {#sltiu}

### Purpose

Compares a register with the unsigned interpretation of a sign-extended 12-bit constant and writes 1 or 0.

### Assembly syntax

```asm
sltiu rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00003013` |

Recognize when `(instruction & 0x0000707F) == 0x00003013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `011` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation SLTIU -->
```python
write_rd(1 if x[rs1] < u32(imm) else 0)
pc = u32(pc + 4)
```

### Exceptions and special cases

Sign extension happens before the unsigned comparison. Thus -1 means 0xFFFFFFFF, not 0x00000FFF. `seqz rd, rs` expands to `sltiu rd, rs, 1`. Compare [SLTI](#slti).

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
sltiu x5, x6, -1
```

Instruction word: `0xFFF33293`. The example is one independent instruction attempt.

<!-- rv32i-example SLTIU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000001` |
| `x6` | `0xFFFFFFFE` | `0xFFFFFFFE` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
