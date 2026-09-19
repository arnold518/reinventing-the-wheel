# Ordering and System

[Instruction index](index.md) · [Machine conventions](../machine.md) · [Instruction formats](../formats.md)

[FENCE](#fence) · [ECALL](#ecall) · [EBREAK](#ebreak)

Operations use the [defined pseudocode helpers](../machine.md#notation-and-operation-helpers). Each entry specifies one successfully decoded instruction; [shared fetch and trap rules](../machine.md#common-exceptions) also apply.

## FENCE — Memory Ordering Fence {#fence}

### Purpose

Orders the selected earlier memory/device accesses before the selected later accesses.

### Assembly syntax

```asm
fence pred, succ
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0x0000707F` |
| Match | `0x0000000F` |

Recognize when `(instruction & 0x0000707F) == 0x0000000F`. Fixed field values below are binary.

| `31:28` | `27:24` | `23:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- | --- | --- |
| `fm` | `pred` | `succ` | `ignored` | `000` | `ignored` | `0001111` |

### Operands

| Operand | Meaning |
| --- | --- |
| `pred` | Earlier access kinds: i=device input, o=device output, r=memory read, w=memory write. |
| `succ` | Later access kinds, using the same four-bit mask. |

### Operation

<!-- rv32i-operation FENCE -->
```python
pc = u32(pc + 4)
```

### Exceptions and special cases

Current simulator policy: serial, single-hart memory has no pending/reordered accesses requiring extra work, so FENCE changes neither registers nor memory data. This does not establish multicore, MMIO, or weak-memory-model support. No cache flush or data transfer is implied.

Bits 31:28 are fm, 27:24 are pred, and 23:20 are succ. In each mask, bits 3/2/1/0 mean device input/output and memory read/write. The base decoder ignores reserved rd/rs1 and mode fields. Reserved modes are treated as ordinary fences, not illegal arithmetic encodings. `FENCE.TSO` (0x8330000F) is accepted as FENCE; stronger full-fence ordering is permitted. `FENCE.I` belongs to Zifencei and is rejected by this core.

### Example

```asm
fence iorw, iorw
```

Instruction word: `0x0FF0000F`. The example is one independent instruction attempt.

<!-- rv32i-example FENCE -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000044` |
| Status | Run | Run |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## ECALL — Environment Call {#ecall}

### Purpose

Raises an environment-call exception to request a service from the execution environment.

### Assembly syntax

```asm
ecall
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0xFFFFFFFF` |
| Match | `0x00000073` |

Recognize when `(instruction & 0xFFFFFFFF) == 0x00000073`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `000000000000` | `00000` | `000` | `00000` | `1110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `None` | No register or immediate operand. All instruction bits are fixed. |

### Operation

<!-- rv32i-operation ECALL -->
```python
trap("EnvironmentCall")
```

### Exceptions and special cases

Current simulator policy: set trapped=true with cause EnvironmentCall and stop at the current PC. Registers and data memory are unchanged. There is no implemented syscall ABI, service handler, trap vector, or resumable privileged state.

### Example

```asm
ecall
```

Instruction word: `0x00000073`. The example is one independent instruction attempt.

<!-- rv32i-example ECALL -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000040` |
| Status | Run | EnvironmentCall |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

## EBREAK — Breakpoint {#ebreak}

### Purpose

Requests a breakpoint exception in the ISA; CircuitSim instead uses it as successful program termination.

### Assembly syntax

```asm
ebreak
```

### Encoding and decoding

| Property | Value |
| --- | --- |
| Format | I |
| Mask | `0xFFFFFFFF` |
| Match | `0x00100073` |

Recognize when `(instruction & 0xFFFFFFFF) == 0x00100073`. Fixed field values below are binary.

| `31:20` | `19:15` | `14:12` | `11:7` | `6:0` |
| --- | --- | --- | --- | --- |
| `000000000001` | `00000` | `000` | `00000` | `1110011` |

### Operands

| Operand | Meaning |
| --- | --- |
| `None` | No register or immediate operand. All instruction bits are fixed. |

### Operation

<!-- rv32i-operation EBREAK -->
```python
halted = True
```

### Exceptions and special cases

Project deviation: set halted=true, leave trapped=false, and stop with PC/registers/memory unchanged. Standard EBREAK raises a breakpoint exception; our halt behavior is a runner convention. RV32I has no separate HALT instruction. This does not return from a function.

### Example

```asm
ebreak
```

Instruction word: `0x00100073`. The example is one independent instruction attempt.

<!-- rv32i-example EBREAK -->
| Item | Before | After |
| --- | --- | --- |
| `PC` | `0x00000040` | `0x00000040` |
| Status | Run | Halt |

All unlisted registers start at zero and remain unchanged. [Full initial-state conventions and executable checks](index.md#example-conventions) apply.

Instruction semantics: [RV32I specification, version 2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). CircuitSim-specific policies are identified in the entries and [machine model](../machine.md).
