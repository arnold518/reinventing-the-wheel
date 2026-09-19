# Program 8: Byte copy and checksum

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Copy five bytes and compute an unsigned-byte checksum, then store the checksum as a word.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program8` |
| Registered test | `RV32ISingleCycleSystemTest/program-08` |
| Instruction image | 14 words, starting at `0x00000000`; last word at `0x00000034` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program8-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `01 02 03 04` |
| `0x00000104` | `FF` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Next source byte |
| `x2` | Next destination byte |
| `x3` | Remaining byte count |
| `x4` | Checksum |
| `x5` | Loaded unsigned byte |
| `x6` | Checksum output address 0x160 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program8-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
addi x2, x0, 0x140             # 0x00000004 | 0x14000113
addi x3, x0, 5                 # 0x00000008 | 0x00500193
addi x4, x0, 0                 # 0x0000000C | 0x00000213
copy_loop:
lbu x5, 0(x1)                  # 0x00000010 | 0x0000C283
sb x5, 0(x2)                   # 0x00000014 | 0x00510023
add x4, x4, x5                 # 0x00000018 | 0x00520233
addi x1, x1, 1                 # 0x0000001C | 0x00108093
addi x2, x2, 1                 # 0x00000020 | 0x00110113
addi x3, x3, -1                # 0x00000024 | 0xFFF18193
bne x3, x0, copy_loop          # 0x00000028 | 0xFE0194E3
addi x6, x0, 0x160             # 0x0000002C | 0x16000313
sw x4, 0(x6)                   # 0x00000030 | 0x00432023
ebreak                         # 0x00000034 | 0x00100073
```

## Execution

1. Initialize source `0x100`, destination `0x140`, count five, and checksum zero.
2. Each iteration uses LBU to read one byte, SB to copy it, and ADD to include its unsigned value in the checksum. Both pointers advance by **one byte**.
3. The final input byte is `0xFF`. LBU contributes 255, not -1; the checksum is `1 + 2 + 3 + 4 + 255 = 265 = 0x109`.
4. After five iterations, store the checksum at `0x160` and halt. The total is `4 + 5 × 7 + 3 = 42` attempts.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program8-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000105` | Next source byte |
| `x2` | `0x00000145` | Next destination byte |
| `x3` | `0x00000000` | Remaining byte count |
| `x4` | `0x00000109` | Checksum |
| `PC` | `0x00000034` | EBREAK address |
| `Attempts` | 42 | Checkpoints I0–I41 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program8-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000140` | `01 02 03 04` |
| `0x00000144` | `FF` |
| `0x00000160` | `09 01 00 00` |

All data bytes not listed as written retain their initial values. The destination bytes are 01 02 03 04 FF; the checksum is the little-endian word 0x00000109.

## Simulation and checkpoints

All 42 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program8 RV32ISingleCycleSystemTest/program-08 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program8&amp;core_fidelity=behavioral" title="RV32I program 8: Byte copy and checksum" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program8&amp;core_fidelity=behavioral" class="md-button">Open program 8 reference</a>
