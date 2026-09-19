# Stores

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[SB](#sb) · [SH](#sh) · [SW](#sw)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## SB — Store Byte {#sb}

### Purpose

Writes the low eight bits of a register to one data-memory byte.

### Assembly syntax

```asm
sb rs2, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | S |
| Mask | `0x0000707F` |
| Match | `0x00000023` |

Recognize when `(instruction & 0x0000707F) == 0x00000023`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `imm[11:5]` | `rs2` | `rs1` | `000` | `imm[4:0]` | `0100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs2` | Data source register number, 0–31; only the selected low bits are stored. |
| `rs1` | Base-address register number, 0–31. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation SB -->
```python
address = u32(x[rs1] + imm)
store8(address, x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

Any byte address is aligned for SB. An inaccessible byte raises StoreAccessFault. Other memory bytes and all registers are unchanged; a store has no rd field.

Alignment is checked before range. A faulting store changes no bytes and does not advance PC. There is no signed/unsigned store distinction: the operation copies low bits. Register x0 can be used as either a zero base or zero data.

### Example

```asm
sb x7, 1(x6)
```

Instruction word: `0x007300A3`. The example is one independent instruction attempt.

<!-- rv32i-example SB -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0x00000100` | `0x00000100` |
| `x7` | `0x000000AB` | `0x000000AB` |
| `data word at 0x100` | `0x80FF7F01` | `0x80FFAB01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SH — Store Halfword {#sh}

### Purpose

Writes the low 16 bits of a register to two data-memory bytes.

### Assembly syntax

```asm
sh rs2, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | S |
| Mask | `0x0000707F` |
| Match | `0x00001023` |

Recognize when `(instruction & 0x0000707F) == 0x00001023`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `imm[11:5]` | `rs2` | `rs1` | `001` | `imm[4:0]` | `0100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs2` | Data source register number, 0–31; only the selected low bits are stored. |
| `rs1` | Base-address register number, 0–31. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation SH -->
```python
address = u32(x[rs1] + imm)
store16(address, x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

The address must be divisible by 2 in this environment; otherwise StoreAddressMisaligned. Either byte outside memory raises StoreAccessFault. The low byte is stored at the lower address.

Alignment is checked before range. A faulting store changes no bytes and does not advance PC. There is no signed/unsigned store distinction: the operation copies low bits. Register x0 can be used as either a zero base or zero data.

### Example

```asm
sh x7, 2(x6)
```

Instruction word: `0x00731123`. The example is one independent instruction attempt.

<!-- rv32i-example SH -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0x00000100` | `0x00000100` |
| `x7` | `0x00001234` | `0x00001234` |
| `data word at 0x100` | `0x80FF7F01` | `0x12347F01` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## SW — Store Word {#sw}

### Purpose

Writes all 32 bits of a register to four data-memory bytes.

### Assembly syntax

```asm
sw rs2, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | S |
| Mask | `0x0000707F` |
| Match | `0x00002023` |

Recognize when `(instruction & 0x0000707F) == 0x00002023`. Fixed field values below are binary.

| `31:25` | `24:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `imm[11:5]` | `rs2` | `rs1` | `010` | `imm[4:0]` | `0100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs2` | Data source register number, 0–31; only the selected low bits are stored. |
| `rs1` | Base-address register number, 0–31. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation SW -->
```python
address = u32(x[rs1] + imm)
store32(address, x[rs2])
pc = u32(pc + 4)
```

### Exceptions and special cases

The address must be divisible by 4 in this environment; otherwise StoreAddressMisaligned. Any of the four bytes outside memory raises StoreAccessFault. Bytes are stored least significant first.

Alignment is checked before range. A faulting store changes no bytes and does not advance PC. There is no signed/unsigned store distinction: the operation copies low bits. Register x0 can be used as either a zero base or zero data.

### Example

```asm
sw x7, 0(x6)
```

Instruction word: `0x00732023`. The example is one independent instruction attempt.

<!-- rv32i-example SW -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0x00000100` | `0x00000100` |
| `x7` | `0x12345678` | `0x12345678` |
| `data word at 0x100` | `0x80FF7F01` | `0x12345678` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. The initial word corresponds to bytes `01 7F FF 80` at `0x100`–`0x103`. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
