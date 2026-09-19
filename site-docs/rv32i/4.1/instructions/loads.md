# Loads

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[LB](#lb) · [LH](#lh) · [LW](#lw) · [LBU](#lbu) · [LHU](#lhu)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## LB — Load Byte Signed {#lb}

### Purpose

Reads one data-memory byte and sign-extends it to a 32-bit register value.

### Assembly syntax

```asm
lb rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00000003` |

Recognize when `(instruction & 0x0000707F) == 0x00000003`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `000` | `rd` | `0000011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation LB -->
```python
address = u32(x[rs1] + imm)
value = sext(read8(address), 8)
write_rd(value)
pc = u32(pc + 4)
```

### Exceptions and special cases

Any byte address is aligned for LB. An inaccessible byte raises LoadAccessFault. Loading 0xFF produces 0xFFFFFFFF (-1); compare [LBU](#lbu).

Alignment is checked before range. On a fault, the destination and PC are unchanged. A load into x0 still performs the memory access and can fault. The base is read before the destination write, including when rd=rs1.

### Example

```asm
lb x5, 2(x6)
```

Instruction word: `0x00230283`. The example is one independent instruction attempt.

<!-- rv32i-example LB -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFFFFFF` |
| `x6` | `0x00000100` | `0x00000100` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FF7F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## LH — Load Halfword Signed {#lh}

### Purpose

Reads two data-memory bytes and sign-extends the 16-bit value to 32 bits.

### Assembly syntax

```asm
lh rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00001003` |

Recognize when `(instruction & 0x0000707F) == 0x00001003`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `001` | `rd` | `0000011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation LH -->
```python
address = u32(x[rs1] + imm)
value = sext(read16(address), 16)
write_rd(value)
pc = u32(pc + 4)
```

### Exceptions and special cases

The address must be divisible by 2 in this environment; otherwise LoadAddressMisaligned. Either byte outside memory raises LoadAccessFault. Compare [LHU](#lhu).

Alignment is checked before range. On a fault, the destination and PC are unchanged. A load into x0 still performs the memory access and can fault. The base is read before the destination write, including when rd=rs1.

### Example

```asm
lh x5, 2(x6)
```

Instruction word: `0x00231283`. The example is one independent instruction attempt.

<!-- rv32i-example LH -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0xFFFF80FF` |
| `x6` | `0x00000100` | `0x00000100` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FF7F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## LW — Load Word {#lw}

### Purpose

Reads four data-memory bytes into a 32-bit register.

### Assembly syntax

```asm
lw rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00002003` |

Recognize when `(instruction & 0x0000707F) == 0x00002003`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `010` | `rd` | `0000011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation LW -->
```python
address = u32(x[rs1] + imm)
value = read32(address)
write_rd(value)
pc = u32(pc + 4)
```

### Exceptions and special cases

The address must be divisible by 4 in this environment; otherwise LoadAddressMisaligned. Any of the four bytes outside memory raises LoadAccessFault. All 32 bits are loaded unchanged; there is no separate signed/unsigned LW distinction in RV32I.

Alignment is checked before range. On a fault, the destination and PC are unchanged. A load into x0 still performs the memory access and can fault. The base is read before the destination write, including when rd=rs1.

### Example

```asm
lw x5, 0(x6)
```

Instruction word: `0x00032283`. The example is one independent instruction attempt.

<!-- rv32i-example LW -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x80FF7F01` |
| `x6` | `0x00000100` | `0x00000100` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FF7F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## LBU — Load Byte Unsigned {#lbu}

### Purpose

Reads one data-memory byte and zero-extends it to a 32-bit register value.

### Assembly syntax

```asm
lbu rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00004003` |

Recognize when `(instruction & 0x0000707F) == 0x00004003`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `100` | `rd` | `0000011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation LBU -->
```python
address = u32(x[rs1] + imm)
write_rd(read8(address))
pc = u32(pc + 4)
```

### Exceptions and special cases

Any byte address is aligned for LBU. An inaccessible byte raises LoadAccessFault. Loading 0xFF produces 0x000000FF (255); compare [LB](#lb).

Alignment is checked before range. On a fault, the destination and PC are unchanged. A load into x0 still performs the memory access and can fault. The base is read before the destination write, including when rd=rs1.

### Example

```asm
lbu x5, 2(x6)
```

Instruction word: `0x00234283`. The example is one independent instruction attempt.

<!-- rv32i-example LBU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x000000FF` |
| `x6` | `0x00000100` | `0x00000100` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FF7F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## LHU — Load Halfword Unsigned {#lhu}

### Purpose

Reads two data-memory bytes and zero-extends the 16-bit value to 32 bits.

### Assembly syntax

```asm
lhu rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00005003` |

Recognize when `(instruction & 0x0000707F) == 0x00005003`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `101` | `rd` | `0000011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation LHU -->
```python
address = u32(x[rs1] + imm)
write_rd(read16(address))
pc = u32(pc + 4)
```

### Exceptions and special cases

The address must be divisible by 2 in this environment; otherwise LoadAddressMisaligned. Either byte outside memory raises LoadAccessFault. Compare [LH](#lh).

Alignment is checked before range. On a fault, the destination and PC are unchanged. A load into x0 still performs the memory access and can fault. The base is read before the destination write, including when rd=rs1.

### Example

```asm
lhu x5, 2(x6)
```

Instruction word: `0x00235283`. The example is one independent instruction attempt.

<!-- rv32i-example LHU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x5` | `0x00000000` | `0x000080FF` |
| `x6` | `0x00000100` | `0x00000100` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FF7F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
