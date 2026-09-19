# Program 2: Arithmetic, logic, shifts, and comparisons

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Exercise register and immediate ALU instructions, signed/unsigned comparisons, and writes to x0; preserve the results in data memory.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program2` |
| Registered test | `RV32ISingleCycleSystemTest/program-02` |
| Instruction image | 35 words, starting at `0x00000000`; last word at `0x00000088` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program2-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Operand 0x55 (85) |
| `x2` | Operand 0x0F (15) |
| `x10` | Signed operand -8 |
| `x3–x9, x11–x17` | Operation results |
| `x20` | Output base at 0x200 |
| `x0` | Attempted destination that must remain zero |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program2-assembly -->
```asm
addi x1, x0, 85                # 0x00000000 | 0x05500093
addi x2, x0, 15                # 0x00000004 | 0x00F00113
addi x10, x0, -8               # 0x00000008 | 0xFF800513
add x3, x1, x2                 # 0x0000000C | 0x002081B3
sub x4, x1, x2                 # 0x00000010 | 0x40208233
and x5, x1, x2                 # 0x00000014 | 0x0020F2B3
or x6, x1, x2                  # 0x00000018 | 0x0020E333
xor x7, x1, x2                 # 0x0000001C | 0x0020C3B3
slli x8, x2, 4                 # 0x00000020 | 0x00411413
srli x9, x8, 2                 # 0x00000024 | 0x00245493
srai x11, x10, 1               # 0x00000028 | 0x40155593
slt x12, x10, x1               # 0x0000002C | 0x00152633
sltu x13, x10, x1              # 0x00000030 | 0x001536B3
xori x14, x1, 51               # 0x00000034 | 0x0330C713
ori x14, x14, 256              # 0x00000038 | 0x10076713
andi x15, x1, 63               # 0x0000003C | 0x03F0F793
slti x16, x10, 0               # 0x00000040 | 0x00052813
sltiu x17, x10, 1              # 0x00000044 | 0x00153893
addi x0, x0, 123               # 0x00000048 | 0x07B00013
addi x20, x0, 0x200            # 0x0000004C | 0x20000A13
sw x3, 0(x20)                  # 0x00000050 | 0x003A2023
sw x4, 4(x20)                  # 0x00000054 | 0x004A2223
sw x5, 8(x20)                  # 0x00000058 | 0x005A2423
sw x6, 12(x20)                 # 0x0000005C | 0x006A2623
sw x7, 16(x20)                 # 0x00000060 | 0x007A2823
sw x8, 20(x20)                 # 0x00000064 | 0x008A2A23
sw x9, 24(x20)                 # 0x00000068 | 0x009A2C23
sw x11, 28(x20)                # 0x0000006C | 0x00BA2E23
sw x12, 32(x20)                # 0x00000070 | 0x02CA2023
sw x13, 36(x20)                # 0x00000074 | 0x02DA2223
sw x14, 40(x20)                # 0x00000078 | 0x02EA2423
sw x15, 44(x20)                # 0x0000007C | 0x02FA2623
sw x16, 48(x20)                # 0x00000080 | 0x030A2823
sw x17, 52(x20)                # 0x00000084 | 0x031A2A23
ebreak                         # 0x00000088 | 0x00100073
```

## Execution

1. Initialize `x1=0x55`, `x2=0x0F`, and `x10=-8`.
2. Compute addition, subtraction, bitwise operations, shifts, and comparisons into separate registers. Logical right shift inserts zero bits; arithmetic right shift preserves the sign of -8, producing -4.
3. Signed comparisons interpret -8 as negative; unsigned comparisons interpret the same bits as `0xFFFFFFF8`. Thus SLT/SLTI produce 1 while the corresponding unsigned tests produce 0.
4. Attempt `addi x0, x0, 123`; x0 remains zero. Store fourteen result words consecutively at `0x200`–`0x234`, then halt.

The stored words, in order, are `0x64, 0x46, 0x05, 0x5F, 0x5A, 0xF0, 0x3C, 0xFFFFFFFC, 1, 0, 0x166, 0x15, 1, 0`. All 35 instructions execute once.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program2-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x0` | `0x00000000` | Attempted destination that must remain zero |
| `x3` | `0x00000064` | Register result |
| `x4` | `0x00000046` | Register result |
| `x8` | `0x000000F0` | Register result |
| `x9` | `0x0000003C` | Register result |
| `x11` | `0xFFFFFFFC` | Register result |
| `x12` | `0x00000001` | Register result |
| `x13` | `0x00000000` | Register result |
| `x14` | `0x00000166` | Register result |
| `x15` | `0x00000015` | Register result |
| `x16` | `0x00000001` | Register result |
| `x17` | `0x00000000` | Register result |
| `PC` | `0x00000088` | EBREAK address |
| `Attempts` | 35 | Checkpoints I0–I34 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program2-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000200` | `64 00 00 00` |
| `0x00000204` | `46 00 00 00` |
| `0x00000208` | `05 00 00 00` |
| `0x0000020C` | `5F 00 00 00` |
| `0x00000210` | `5A 00 00 00` |
| `0x00000214` | `F0 00 00 00` |
| `0x00000218` | `3C 00 00 00` |
| `0x0000021C` | `FC FF FF FF` |
| `0x00000220` | `01 00 00 00` |
| `0x00000224` | `00 00 00 00` |
| `0x00000228` | `66 01 00 00` |
| `0x0000022C` | `15 00 00 00` |
| `0x00000230` | `01 00 00 00` |
| `0x00000234` | `00 00 00 00` |

All data bytes not listed as written retain their initial values. The output region contains fourteen little-endian words. Some registers holding intermediate operands/results are intentionally absent from the fixture's selected-register assertions.

## Simulation and checkpoints

All 35 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program2 RV32ISingleCycleSystemTest/program-02 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program2&amp;core_fidelity=behavioral" title="RV32I program 2: Arithmetic, logic, shifts, and comparisons" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program2&amp;core_fidelity=behavioral" class="md-button">Open program 2 reference</a>
