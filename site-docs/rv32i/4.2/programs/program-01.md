# Program 1: Array sum, loop, and stores

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Sum four words using a counted loop, then store results at word, byte, and halfword widths.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-01` |
| Headless selection | `--program 1` |
| Visualizer scenario | `rv32i-program1` |
| Registered test | `RV32ISingleCycleSystemTest/program-01` |
| Instruction image | 14 words, starting at `0x00000000`; last word at `0x00000034` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program1-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `03 00 00 00` |
| `0x00000104` | `05 00 00 00` |
| `0x00000108` | `07 00 00 00` |
| `0x0000010C` | `0B 00 00 00` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Next input address |
| `x2` | Remaining word count, not a stack pointer |
| `x3` | Sum |
| `x4` | Loaded word |
| `x5` | Output base at 0x120 |
| `x6` | Sum plus 0x34 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program1-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
addi x2, x0, 4                 # 0x00000004 | 0x00400113
addi x3, x0, 0                 # 0x00000008 | 0x00000193
loop:
lw x4, 0(x1)                   # 0x0000000C | 0x0000A203
add x3, x3, x4                 # 0x00000010 | 0x004181B3
addi x1, x1, 4                 # 0x00000014 | 0x00408093
addi x2, x2, -1                # 0x00000018 | 0xFFF10113
bne x2, x0, loop               # 0x0000001C | 0xFE0118E3
addi x5, x0, 0x120             # 0x00000020 | 0x12000293
sw x3, 0(x5)                   # 0x00000024 | 0x0032A023
sb x3, 4(x5)                   # 0x00000028 | 0x00328223
addi x6, x3, 52                # 0x0000002C | 0x03418313
sh x6, 6(x5)                   # 0x00000030 | 0x00629323
ebreak                         # 0x00000034 | 0x00100073
```

## Execution

1. `0x00`–`0x08` initialize the source pointer, count, and sum.
2. Each pass through `0x0C`–`0x1C` loads one word, adds it to `x3`, advances the pointer by four bytes, and decrements the count. BNE returns to `loop` while the count is nonzero.
3. After the fourth pass, the branch falls through to `0x20`. The program writes the sum at `0x120`, its low byte at `0x124`, and the low halfword of `sum + 0x34` at `0x126`.
4. EBREAK at `0x34` halts without advancing PC.

The loop state after each BNE makes the accumulated result explicit:

<!-- rv32i-program1-loop -->
| Checkpoint | Loaded word | `x1` | `x2` | `x3` | Next PC |
| --- | --- | --- | --- | --- | --- |
| I7 | 3 | `0x104` | 3 | 3 | `0x0C` |
| I12 | 5 | `0x108` | 2 | 8 | `0x0C` |
| I17 | 7 | `0x10C` | 1 | 15 | `0x0C` |
| I22 | 11 | `0x110` | 0 | 26 | `0x20` |

The total is **29 attempts**: three setup instructions, four passes of five instructions, and six final instructions including EBREAK. The halfword result is `26 + 0x34 = 78`; it is not a multiplication.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program1-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000110` | Next input address |
| `x2` | `0x00000000` | Remaining word count, not a stack pointer |
| `x3` | `0x0000001A` | Sum |
| `x5` | `0x00000120` | Output base at 0x120 |
| `x6` | `0x0000004E` | Sum plus 0x34 |
| `PC` | `0x00000034` | EBREAK address |
| `Attempts` | 29 | Checkpoints I0–I28 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program1-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `1A 00 00 00` |
| `0x00000124` | `1A` |
| `0x00000126` | `4E 00` |

All data bytes not listed as written retain their initial values. The input words are 3, 5, 7, and 11. The halfword output is 26 + 0x34 = 78. Byte 0x125 is never written.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 1 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 29 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program1 RV32ISingleCycleSystemTest/program-01 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program1&amp;core_fidelity=behavioral" title="RV32I program 1: Array sum, loop, and stores" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program1&amp;core_fidelity=behavioral" class="md-button">Open program 1 reference</a>
