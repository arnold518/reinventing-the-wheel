# Program 3: Signed and unsigned memory accesses

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Load the same byte/halfword patterns with and without sign extension, then verify word and partial-width stores.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program3` |
| Registered test | `RV32ISingleCycleSystemTest/program-03` |
| Instruction image | 17 words, starting at `0x00000000`; last word at `0x00000040` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program3-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `80 7F 00 00` |
| `0x00000104` | `80 FF 00 00` |
| `0x00000108` | `21 43 65 87` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Input base at 0x100 |
| `x2, x3` | Signed/unsigned loads of byte 0x80 |
| `x4` | Signed load of byte 0x7F |
| `x5, x6` | Signed/unsigned loads of halfword 0xFF80 |
| `x7` | Loaded word 0x87654321 |
| `x8` | Output base at 0x120 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program3-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
lb x2, 0(x1)                   # 0x00000004 | 0x00008103
lbu x3, 0(x1)                  # 0x00000008 | 0x0000C183
lb x4, 1(x1)                   # 0x0000000C | 0x00108203
lh x5, 4(x1)                   # 0x00000010 | 0x00409283
lhu x6, 4(x1)                  # 0x00000014 | 0x0040D303
lw x7, 8(x1)                   # 0x00000018 | 0x0080A383
addi x8, x0, 0x120             # 0x0000001C | 0x12000413
sw x2, 0(x8)                   # 0x00000020 | 0x00242023
sw x3, 4(x8)                   # 0x00000024 | 0x00342223
sw x4, 8(x8)                   # 0x00000028 | 0x00442423
sw x5, 12(x8)                  # 0x0000002C | 0x00542623
sw x6, 16(x8)                  # 0x00000030 | 0x00642823
sw x7, 20(x8)                  # 0x00000034 | 0x00742A23
sb x3, 24(x8)                  # 0x00000038 | 0x00340C23
sh x6, 26(x8)                  # 0x0000003C | 0x00641D23
ebreak                         # 0x00000040 | 0x00100073
```

## Execution

1. Read byte `0x80` using LB and LBU: the results are `0xFFFFFF80` (-128) and `0x00000080` (128).
2. Read byte `0x7F` with LB; its sign bit is clear, so the result is 127.
3. Read the little-endian halfword at `0x104`. LH turns `0xFF80` into `0xFFFFFF80`; LHU turns it into `0x0000FF80`.
4. Read the word at `0x108` as `0x87654321`, then store the six load results as words at `0x120`–`0x134`.
5. SB writes only byte `0x138`; SH writes only bytes `0x13A`–`0x13B`. Byte `0x139` remains zero. EBREAK ends the 17-instruction sequence.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program3-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x2` | `0xFFFFFF80` | Register result |
| `x3` | `0x00000080` | Register result |
| `x4` | `0x0000007F` | Signed load of byte 0x7F |
| `x5` | `0xFFFFFF80` | Register result |
| `x6` | `0x0000FF80` | Register result |
| `x7` | `0x87654321` | Loaded word 0x87654321 |
| `PC` | `0x00000040` | EBREAK address |
| `Attempts` | 17 | Checkpoints I0–I16 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program3-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `80 FF FF FF` |
| `0x00000124` | `80 00 00 00` |
| `0x00000128` | `7F 00 00 00` |
| `0x0000012C` | `80 FF FF FF` |
| `0x00000130` | `80 FF 00 00` |
| `0x00000134` | `21 43 65 87` |
| `0x00000138` | `80` |
| `0x0000013A` | `80 FF` |

All data bytes not listed as written retain their initial values. Input bytes are unchanged. Partial-width stores do not overwrite the neighboring zero byte at 0x139.

## Simulation and checkpoints

All 17 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program3 RV32ISingleCycleSystemTest/program-03 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=behavioral" title="RV32I program 3: Signed and unsigned memory accesses" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program3&amp;core_fidelity=behavioral" class="md-button">Open program 3 reference</a>
