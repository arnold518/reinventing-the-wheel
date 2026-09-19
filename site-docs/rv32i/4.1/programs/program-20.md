# Program 20: Branch-heavy loop

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Provide frequent taken branches interleaved with two simple ALU updates.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program20` |
| Registered test | `RV32ISingleCycleSystemTest/program-20` |
| Instruction image | 6 words, starting at `0x00000000`; last word at `0x00000014` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program20-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Countdown from 16 |
| `x2` | Accumulator counting iterations |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program20-assembly -->
```asm
addi x1, x0, 16                # 0x00000000 | 0x01000093
addi x2, x0, 0                 # 0x00000004 | 0x00000113
loop:
addi x2, x2, 1                 # 0x00000008 | 0x00110113
addi x1, x1, -1                # 0x0000000C | 0xFFF08093
bne x1, x0, loop               # 0x00000010 | 0xFE009CE3
ebreak                         # 0x00000014 | 0x00100073
```

## Execution

1. Initialize count 16 and accumulator zero.
2. Each iteration increments `x2`, decrements `x1`, and executes BNE back to the first update while the count is nonzero.
3. The branch is taken fifteen times and falls through on the sixteenth. The final values are `x1=0` and `x2=16`.
4. EBREAK completes `2 + 16 × 3 + 1 = 51` attempts.

One branch per three loop instructions makes redirects frequent. The reference does not assign a pipeline flush penalty to them.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program20-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000000` | Countdown from 16 |
| `x2` | `0x00000010` | Accumulator counting iterations |
| `PC` | `0x00000014` | EBREAK address |
| `Attempts` | 51 | Checkpoints I0–I50 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program20-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. All data memory remains unchanged; there are no loads or stores.

## Simulation and checkpoints

All 51 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program20 RV32ISingleCycleSystemTest/program-20 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program20&amp;core_fidelity=behavioral" title="RV32I program 20: Branch-heavy loop" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program20&amp;core_fidelity=behavioral" class="md-button">Open program 20 reference</a>
