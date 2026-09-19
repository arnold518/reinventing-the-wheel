# Logic

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[AND](#and) · [OR](#or) · [XOR](#xor) · [ANDI](#andi) · [ORI](#ori) · [XORI](#xori)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## AND — Bitwise AND {#and}

### Purpose

Writes the bitwise AND of two register values.

### Assembly syntax

```asm
and rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00007033` |

Recognize when `(instruction & 0xFE00707F) == 0x00007033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `111` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation AND -->
```python
write_rd(x[rs1] & x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Each result bit is one only when both corresponding source bits are one. This is not a whole-word Boolean AND.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
and x5, x6, x7
```

Instruction word: `0x007372B3`. The example is one independent instruction attempt.

<!-- rv32i-example AND -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x0000000A` |
| `x6` | `0x000000AA` | `0x000000AA` |
| `x7` | `0x0000000F` | `0x0000000F` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## OR — Bitwise OR {#or}

### Purpose

Writes the bitwise OR of two register values.

### Assembly syntax

```asm
or rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00006033` |

Recognize when `(instruction & 0xFE00707F) == 0x00006033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `110` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation OR -->
```python
write_rd(x[rs1] | x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Each result bit is one when either corresponding source bit is one.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
or x5, x6, x7
```

Instruction word: `0x007362B3`. The example is one independent instruction attempt.

<!-- rv32i-example OR -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x000000AF` |
| `x6` | `0x000000A0` | `0x000000A0` |
| `x7` | `0x0000000F` | `0x0000000F` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## XOR — Bitwise Exclusive OR {#xor}

### Purpose

Writes the bitwise exclusive OR of two register values.

### Assembly syntax

```asm
xor rd, rs1, rs2
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | R |
| Mask | `0xFE00707F` |
| Match | `0x00004033` |

Recognize when `(instruction & 0xFE00707F) == 0x00004033`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `0000000` | `rs2` | `rs1` | `100` | `rd` | `0110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |

### Operation

<!-- rv32i-operation XOR -->
```python
write_rd(x[rs1] ^ x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Each result bit is one when the corresponding source bits differ.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
xor x5, x6, x7
```

Instruction word: `0x007342B3`. The example is one independent instruction attempt.

<!-- rv32i-example XOR -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x000000A5` |
| `x6` | `0x000000AA` | `0x000000AA` |
| `x7` | `0x0000000F` | `0x0000000F` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## ANDI — Bitwise AND Immediate {#andi}

### Purpose

Writes the bitwise AND of a register and a sign-extended 12-bit constant.

### Assembly syntax

```asm
andi rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00007013` |

Recognize when `(instruction & 0x0000707F) == 0x00007013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `111` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation ANDI -->
```python
write_rd(x[rs1] & u32(imm))
pc = u32(pc + 4)
```

### Exceptions and special cases

The mask is sign-extended, not zero-extended. A negative immediate supplies ones in its upper 20 bits.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
andi x5, x6, 2047
```

Instruction word: `0x7FF37293`. The example is one independent instruction attempt.

<!-- rv32i-example ANDI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x00000678` |
| `x6` | `0x12345678` | `0x12345678` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## ORI — Bitwise OR Immediate {#ori}

### Purpose

Writes the bitwise OR of a register and a sign-extended 12-bit constant.

### Assembly syntax

```asm
ori rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00006013` |

Recognize when `(instruction & 0x0000707F) == 0x00006013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `110` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation ORI -->
```python
write_rd(x[rs1] | u32(imm))
pc = u32(pc + 4)
```

### Exceptions and special cases

A negative immediate supplies ones in its upper 20 bits. The example uses -2048, whose extended word is 0xFFFFF800.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
ori x5, x6, -2048
```

Instruction word: `0x80036293`. The example is one independent instruction attempt.

<!-- rv32i-example ORI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFFF801` |
| `x6` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## XORI — Bitwise Exclusive OR Immediate {#xori}

### Purpose

Writes the bitwise exclusive OR of a register and a sign-extended 12-bit constant.

### Assembly syntax

```asm
xori rd, rs1, imm12
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00004013` |

Recognize when `(instruction & 0x0000707F) == 0x00004013`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `100` | `rd` | `0010011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `imm12` | Signed constant, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation XORI -->
```python
write_rd(x[rs1] ^ u32(imm))
pc = u32(pc + 4)
```

### Exceptions and special cases

An immediate of -1 becomes 0xFFFFFFFF and complements all 32 source bits: `not rd, rs` is the alias `xori rd, rs, -1`.

This instruction has no instruction-specific execution trap. Writes to x0 are discarded, and source/destination overlap is permitted; source values are read before the write.

### Example

```asm
xori x5, x6, -1
```

Instruction word: `0xFFF34293`. The example is one independent instruction attempt.

<!-- rv32i-example XORI -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xEDCBA987` |
| `x6` | `0x12345678` | `0x12345678` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
