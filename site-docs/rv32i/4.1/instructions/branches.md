# Branches

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[BEQ](#beq) · [BNE](#bne) · [BLT](#blt) · [BGE](#bge) · [BLTU](#bltu) · [BGEU](#bgeu)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## BEQ — Branch if Equal {#beq}

### Purpose

Branches to a PC-relative target when the two source values are equal; otherwise continues at PC+4.

### Assembly syntax

```asm
beq rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00000063` |

Recognize when `(instruction & 0x0000707F) == 0x00000063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `000` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BEQ -->
```python
if x[rs1] == x[rs2]:
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
beq x6, x7, .+8
```

Instruction word: `0x00730463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BEQ -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000048` |
| `x6` | `0x00000005` | `0x00000005` |
| `x7` | `0x00000005` | `0x00000005` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## BNE — Branch if Not Equal {#bne}

### Purpose

Branches to a PC-relative target when the two source values are different; otherwise continues at PC+4.

### Assembly syntax

```asm
bne rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00001063` |

Recognize when `(instruction & 0x0000707F) == 0x00001063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `001` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BNE -->
```python
if x[rs1] != x[rs2]:
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
bne x6, x7, .+8
```

Instruction word: `0x00731463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BNE -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0x00000005` | `0x00000005` |
| `x7` | `0x00000005` | `0x00000005` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## BLT — Branch if Less Than Signed {#blt}

### Purpose

Branches to a PC-relative target when the two source values are in signed less-than order; otherwise continues at PC+4.

### Assembly syntax

```asm
blt rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00004063` |

Recognize when `(instruction & 0x0000707F) == 0x00004063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `100` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BLT -->
```python
if s32(x[rs1]) < s32(x[rs2]):
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
blt x6, x7, .+8
```

Instruction word: `0x00734463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BLT -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000048` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## BGE — Branch if Greater or Equal Signed {#bge}

### Purpose

Branches to a PC-relative target when the two source values are in signed greater-than-or-equal order; otherwise continues at PC+4.

### Assembly syntax

```asm
bge rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00005063` |

Recognize when `(instruction & 0x0000707F) == 0x00005063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `101` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BGE -->
```python
if s32(x[rs1]) >= s32(x[rs2]):
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
bge x6, x7, .+8
```

Instruction word: `0x00735463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BGE -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## BLTU — Branch if Less Than Unsigned {#bltu}

### Purpose

Branches to a PC-relative target when the two source values are in unsigned less-than order; otherwise continues at PC+4.

### Assembly syntax

```asm
bltu rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00006063` |

Recognize when `(instruction & 0x0000707F) == 0x00006063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `110` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BLTU -->
```python
if x[rs1] < x[rs2]:
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
bltu x6, x7, .+8
```

Instruction word: `0x00736463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BLTU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## BGEU — Branch if Greater or Equal Unsigned {#bgeu}

### Purpose

Branches to a PC-relative target when the two source values are in unsigned greater-than-or-equal order; otherwise continues at PC+4.

### Assembly syntax

```asm
bgeu rs1, rs2, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | B |
| Mask | `0x0000707F` |
| Match | `0x00007063` |

Recognize when `(instruction & 0x0000707F) == 0x00007063`. Fixed field values below are binary.

| `31` | `30:25` | `24:20` | `19:15` | `14:12` | `11:8` | `7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `imm[12]` | `imm[10:5]` | `rs2` | `rs1` | `111` | `imm[4:1]` | `imm[11]` | `1100011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rs1` | First source register number, 0–31; read its old 32-bit value. |
| `rs2` | Second source register number, 0–31; read its old 32-bit value. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −4096 through 4094. |

### Operation

<!-- rv32i-operation BGEU -->
```python
if x[rs1] >= x[rs2]:
    target = u32(pc + imm)
    check_target(target)
    pc = target
else:
    pc = u32(pc + 4)
```

### Exceptions and special cases

No destination register or condition-code state is written. The registers are compared directly, without a preceding flag-setting instruction. A taken misaligned target raises InstructionAddressMisaligned; an untaken branch does not check its unused target. An aligned inaccessible target faults on the next fetch. There is no delay slot.

### Example

```asm
bgeu x6, x7, .+8
```

Instruction word: `0x00737463`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example BGEU -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000048` |
| `x6` | `0xFFFFFFFF` | `0xFFFFFFFF` |
| `x7` | `0x00000001` | `0x00000001` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
