# Jumps

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[JAL](#jal) · [JALR](#jalr)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## JAL — Jump and Link {#jal}

### Purpose

Unconditionally jumps to a PC-relative target and saves the following instruction address in rd.

### Assembly syntax

```asm
jal rd, target
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | J |
| Mask | `0x0000007F` |
| Match | `0x0000006F` |

Recognize when `(instruction & 0x0000007F) == 0x0000006F`. Fixed field values below are binary.

| `31` | `30:21` | `20` | `19:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- |
| `imm[20]` | `imm[10:1]` | `imm[11]` | `imm[19:12]` | `rd` | `1101111` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `target` | Assembly label relative to this instruction. Decoded imm is an even byte offset from −1,048,576 through 1,048,574. |

### Operation

<!-- rv32i-operation JAL -->
```python
target = u32(pc + imm)
check_target(target)
write_rd(pc + 4)
pc = target
```

### Exceptions and special cases

A misaligned target raises InstructionAddressMisaligned before the link is written. An aligned target outside instruction memory completes the jump and link write; the next fetch faults. With rd=x0, the jump still happens but the link is discarded. There is no automatic stack push.

### Example

```asm
jal x5, .+8
```

Instruction word: `0x008002EF`. The label `.+8` is address `0x48` when PC is `0x40`. The example is one independent instruction attempt.

<!-- rv32i-example JAL -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000048` |
| `x5` | `0x00000000` | `0x00000044` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## JALR — Jump and Link Register {#jalr}

### Purpose

Unconditionally jumps to a register-relative target and saves the following instruction address in rd.

### Assembly syntax

```asm
jalr rd, offset(rs1)
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x00000067` |

Recognize when `(instruction & 0x0000707F) == 0x00000067`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `imm[11:0]` | `rs1` | `000` | `rd` | `1100111` |

### Operands

| Operand | Meaning |
| --- | --- |
| `rd` | Destination register number, 0–31; a write to x0 is discarded. |
| `rs1` | Base-address register number, 0–31; read its old value. |
| `offset` | Signed byte offset, −2048…2047; decoded imm is sign-extended. |

### Operation

<!-- rv32i-operation JALR -->
```python
target = u32(x[rs1] + imm) & 0xFFFFFFFE
check_target(target)
write_rd(pc + 4)
pc = target
```

### Exceptions and special cases

Clear only target bit 0 after addition. If bit 1 remains set, InstructionAddressMisaligned occurs before writing rd. Read the old rs1 even when rd=rs1. An aligned inaccessible target faults on the next fetch, after the link write. `ret` is `jalr x0, 0(x1)`; no memory pop occurs.

### Example

```asm
jalr x5, 3(x6)
```

Instruction word: `0x003302E7`. The example is one independent instruction attempt.

<!-- rv32i-example JALR -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000048` |
| `x5` | `0x00000000` | `0x00000044` |
| `x6` | `0x00000046` | `0x00000046` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
