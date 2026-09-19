# Program 7: Fibonacci sequence

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Generate and store the first eight Fibonacci values using a loop and register copies.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program7` |
| Registered test | `RV32ISingleCycleSystemTest/program-07` |
| Instruction image | 12 words, starting at `0x00000000`; last word at `0x0000002C` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program7-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Next output address |
| `x2` | Remaining output count |
| `x3` | Current Fibonacci value a |
| `x4` | Next value b |
| `x5` | Temporary a+b |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program7-assembly -->
```asm
addi x1, x0, 0x120             # 0x00000000 | 0x12000093
addi x2, x0, 8                 # 0x00000004 | 0x00800113
addi x3, x0, 0                 # 0x00000008 | 0x00000193
addi x4, x0, 1                 # 0x0000000C | 0x00100213
fib_loop:
sw x3, 0(x1)                   # 0x00000010 | 0x0030A023
add x5, x3, x4                 # 0x00000014 | 0x004182B3
addi x3, x4, 0                 # 0x00000018 | 0x00020193
addi x4, x5, 0                 # 0x0000001C | 0x00028213
addi x1, x1, 4                 # 0x00000020 | 0x00408093
addi x2, x2, -1                # 0x00000024 | 0xFFF10113
bne x2, x0, fib_loop           # 0x00000028 | 0xFE0114E3
ebreak                         # 0x0000002C | 0x00100073
```

## Execution

1. Initialize the destination to `0x120`, count to eight, and pair `(a,b)` to `(0,1)`.
2. Each iteration stores the old `a`, computes `a+b`, then updates the pair to `(b,a+b)`. ADDI with immediate zero performs the register copies.
3. Advance the output pointer by four bytes, decrement the count, and repeat while nonzero.
4. The eight stored words are `0, 1, 1, 2, 3, 5, 8, 13`. The final register pair is `(21,34)` because the update still happens after storing the eighth value.
5. Halt after `4 + 8 × 7 + 1 = 61` attempts. The pointer ends at `0x140`, just past the output array.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program7-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000140` | Next output address |
| `x2` | `0x00000000` | Remaining output count |
| `x3` | `0x00000015` | Current Fibonacci value a |
| `x4` | `0x00000022` | Next value b |
| `PC` | `0x0000002C` | EBREAK address |
| `Attempts` | 61 | Checkpoints I0–I60 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program7-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `00 00 00 00` |
| `0x00000124` | `01 00 00 00` |
| `0x00000128` | `01 00 00 00` |
| `0x0000012C` | `02 00 00 00` |
| `0x00000130` | `03 00 00 00` |
| `0x00000134` | `05 00 00 00` |
| `0x00000138` | `08 00 00 00` |
| `0x0000013C` | `0D 00 00 00` |

All data bytes not listed as written retain their initial values. The final x3/x4 values are the next pair, not the last two stored words.

## Simulation and checkpoints

All 61 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program7 RV32ISingleCycleSystemTest/program-07 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program7&amp;core_fidelity=behavioral" title="RV32I program 7: Fibonacci sequence" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program7&amp;core_fidelity=behavioral" class="md-button">Open program 7 reference</a>
