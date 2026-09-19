# Program 18: Dependent ALU chain

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Provide an adjacent read-after-write dependency chain for later forwarding and throughput comparisons.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program18` |
| Registered test | `RV32ISingleCycleSystemTest/program-18` |
| Instruction image | 66 words, starting at `0x00000000`; last word at `0x00000104` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program18-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Single accumulator, starting at 1 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image. The repeated blocks are fully expanded; there are no omitted instructions or runtime loop directives.

<!-- rv32i-program18-assembly -->
```asm
addi x1, x0, 1                 # 0x00000000 | 0x00100093
addi x1, x1, 1                 # 0x00000004 | 0x00108093
addi x1, x1, 1                 # 0x00000008 | 0x00108093
addi x1, x1, 1                 # 0x0000000C | 0x00108093
addi x1, x1, 1                 # 0x00000010 | 0x00108093
addi x1, x1, 1                 # 0x00000014 | 0x00108093
addi x1, x1, 1                 # 0x00000018 | 0x00108093
addi x1, x1, 1                 # 0x0000001C | 0x00108093
addi x1, x1, 1                 # 0x00000020 | 0x00108093
addi x1, x1, 1                 # 0x00000024 | 0x00108093
addi x1, x1, 1                 # 0x00000028 | 0x00108093
addi x1, x1, 1                 # 0x0000002C | 0x00108093
addi x1, x1, 1                 # 0x00000030 | 0x00108093
addi x1, x1, 1                 # 0x00000034 | 0x00108093
addi x1, x1, 1                 # 0x00000038 | 0x00108093
addi x1, x1, 1                 # 0x0000003C | 0x00108093
addi x1, x1, 1                 # 0x00000040 | 0x00108093
addi x1, x1, 1                 # 0x00000044 | 0x00108093
addi x1, x1, 1                 # 0x00000048 | 0x00108093
addi x1, x1, 1                 # 0x0000004C | 0x00108093
addi x1, x1, 1                 # 0x00000050 | 0x00108093
addi x1, x1, 1                 # 0x00000054 | 0x00108093
addi x1, x1, 1                 # 0x00000058 | 0x00108093
addi x1, x1, 1                 # 0x0000005C | 0x00108093
addi x1, x1, 1                 # 0x00000060 | 0x00108093
addi x1, x1, 1                 # 0x00000064 | 0x00108093
addi x1, x1, 1                 # 0x00000068 | 0x00108093
addi x1, x1, 1                 # 0x0000006C | 0x00108093
addi x1, x1, 1                 # 0x00000070 | 0x00108093
addi x1, x1, 1                 # 0x00000074 | 0x00108093
addi x1, x1, 1                 # 0x00000078 | 0x00108093
addi x1, x1, 1                 # 0x0000007C | 0x00108093
addi x1, x1, 1                 # 0x00000080 | 0x00108093
addi x1, x1, 1                 # 0x00000084 | 0x00108093
addi x1, x1, 1                 # 0x00000088 | 0x00108093
addi x1, x1, 1                 # 0x0000008C | 0x00108093
addi x1, x1, 1                 # 0x00000090 | 0x00108093
addi x1, x1, 1                 # 0x00000094 | 0x00108093
addi x1, x1, 1                 # 0x00000098 | 0x00108093
addi x1, x1, 1                 # 0x0000009C | 0x00108093
addi x1, x1, 1                 # 0x000000A0 | 0x00108093
addi x1, x1, 1                 # 0x000000A4 | 0x00108093
addi x1, x1, 1                 # 0x000000A8 | 0x00108093
addi x1, x1, 1                 # 0x000000AC | 0x00108093
addi x1, x1, 1                 # 0x000000B0 | 0x00108093
addi x1, x1, 1                 # 0x000000B4 | 0x00108093
addi x1, x1, 1                 # 0x000000B8 | 0x00108093
addi x1, x1, 1                 # 0x000000BC | 0x00108093
addi x1, x1, 1                 # 0x000000C0 | 0x00108093
addi x1, x1, 1                 # 0x000000C4 | 0x00108093
addi x1, x1, 1                 # 0x000000C8 | 0x00108093
addi x1, x1, 1                 # 0x000000CC | 0x00108093
addi x1, x1, 1                 # 0x000000D0 | 0x00108093
addi x1, x1, 1                 # 0x000000D4 | 0x00108093
addi x1, x1, 1                 # 0x000000D8 | 0x00108093
addi x1, x1, 1                 # 0x000000DC | 0x00108093
addi x1, x1, 1                 # 0x000000E0 | 0x00108093
addi x1, x1, 1                 # 0x000000E4 | 0x00108093
addi x1, x1, 1                 # 0x000000E8 | 0x00108093
addi x1, x1, 1                 # 0x000000EC | 0x00108093
addi x1, x1, 1                 # 0x000000F0 | 0x00108093
addi x1, x1, 1                 # 0x000000F4 | 0x00108093
addi x1, x1, 1                 # 0x000000F8 | 0x00108093
addi x1, x1, 1                 # 0x000000FC | 0x00108093
addi x1, x1, 1                 # 0x00000100 | 0x00108093
ebreak                         # 0x00000104 | 0x00100073
```

## Execution

1. Initialize `x1=1`.
2. Execute 64 unrolled copies of `addi x1, x1, 1`. Every instruction consumes the result of its immediate predecessor.
3. The final value is 65. EBREAK makes the total `1 + 64 + 1 = 66` attempts.

Unlike Program 17, all updates share one register. There is still no branch loop and no memory traffic. Matching this result establishes instruction effects, not whether a later pipeline avoids stalls.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program18-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000041` | Single accumulator, starting at 1 |
| `PC` | `0x00000104` | EBREAK address |
| `Attempts` | 66 | Checkpoints I0–I65 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program18-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. The repeated assembly is fully expanded so every loaded instruction address can be matched to a checkpoint.

## Simulation and checkpoints

All 66 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program18 RV32ISingleCycleSystemTest/program-18 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program18&amp;core_fidelity=behavioral" title="RV32I program 18: Dependent ALU chain" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program18&amp;core_fidelity=behavioral" class="md-button">Open program 18 reference</a>
