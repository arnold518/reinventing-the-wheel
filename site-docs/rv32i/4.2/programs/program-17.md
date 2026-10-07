# Program 17: Independent ALU chains

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Provide a workload of interleaved, independent accumulator chains for later throughput comparisons.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-17` |
| Headless selection | `--program 17` |
| Visualizer scenario | `rv32i-program17` |
| Registered test | `RV32ISingleCycleSystemTest/program-17` |
| Instruction image | 73 words, starting at `0x00000000`; last word at `0x00000120` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program17-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1–x8` | Eight accumulator chains; register xi starts at i and increments by i |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image. The repeated blocks are fully expanded; there are no omitted instructions or runtime loop directives.

<!-- rv32i-program17-assembly -->
```asm
addi x1, x0, 1                 # 0x00000000 | 0x00100093
addi x2, x0, 2                 # 0x00000004 | 0x00200113
addi x3, x0, 3                 # 0x00000008 | 0x00300193
addi x4, x0, 4                 # 0x0000000C | 0x00400213
addi x5, x0, 5                 # 0x00000010 | 0x00500293
addi x6, x0, 6                 # 0x00000014 | 0x00600313
addi x7, x0, 7                 # 0x00000018 | 0x00700393
addi x8, x0, 8                 # 0x0000001C | 0x00800413
addi x1, x1, 1                 # 0x00000020 | 0x00108093
addi x2, x2, 2                 # 0x00000024 | 0x00210113
addi x3, x3, 3                 # 0x00000028 | 0x00318193
addi x4, x4, 4                 # 0x0000002C | 0x00420213
addi x5, x5, 5                 # 0x00000030 | 0x00528293
addi x6, x6, 6                 # 0x00000034 | 0x00630313
addi x7, x7, 7                 # 0x00000038 | 0x00738393
addi x8, x8, 8                 # 0x0000003C | 0x00840413
addi x1, x1, 1                 # 0x00000040 | 0x00108093
addi x2, x2, 2                 # 0x00000044 | 0x00210113
addi x3, x3, 3                 # 0x00000048 | 0x00318193
addi x4, x4, 4                 # 0x0000004C | 0x00420213
addi x5, x5, 5                 # 0x00000050 | 0x00528293
addi x6, x6, 6                 # 0x00000054 | 0x00630313
addi x7, x7, 7                 # 0x00000058 | 0x00738393
addi x8, x8, 8                 # 0x0000005C | 0x00840413
addi x1, x1, 1                 # 0x00000060 | 0x00108093
addi x2, x2, 2                 # 0x00000064 | 0x00210113
addi x3, x3, 3                 # 0x00000068 | 0x00318193
addi x4, x4, 4                 # 0x0000006C | 0x00420213
addi x5, x5, 5                 # 0x00000070 | 0x00528293
addi x6, x6, 6                 # 0x00000074 | 0x00630313
addi x7, x7, 7                 # 0x00000078 | 0x00738393
addi x8, x8, 8                 # 0x0000007C | 0x00840413
addi x1, x1, 1                 # 0x00000080 | 0x00108093
addi x2, x2, 2                 # 0x00000084 | 0x00210113
addi x3, x3, 3                 # 0x00000088 | 0x00318193
addi x4, x4, 4                 # 0x0000008C | 0x00420213
addi x5, x5, 5                 # 0x00000090 | 0x00528293
addi x6, x6, 6                 # 0x00000094 | 0x00630313
addi x7, x7, 7                 # 0x00000098 | 0x00738393
addi x8, x8, 8                 # 0x0000009C | 0x00840413
addi x1, x1, 1                 # 0x000000A0 | 0x00108093
addi x2, x2, 2                 # 0x000000A4 | 0x00210113
addi x3, x3, 3                 # 0x000000A8 | 0x00318193
addi x4, x4, 4                 # 0x000000AC | 0x00420213
addi x5, x5, 5                 # 0x000000B0 | 0x00528293
addi x6, x6, 6                 # 0x000000B4 | 0x00630313
addi x7, x7, 7                 # 0x000000B8 | 0x00738393
addi x8, x8, 8                 # 0x000000BC | 0x00840413
addi x1, x1, 1                 # 0x000000C0 | 0x00108093
addi x2, x2, 2                 # 0x000000C4 | 0x00210113
addi x3, x3, 3                 # 0x000000C8 | 0x00318193
addi x4, x4, 4                 # 0x000000CC | 0x00420213
addi x5, x5, 5                 # 0x000000D0 | 0x00528293
addi x6, x6, 6                 # 0x000000D4 | 0x00630313
addi x7, x7, 7                 # 0x000000D8 | 0x00738393
addi x8, x8, 8                 # 0x000000DC | 0x00840413
addi x1, x1, 1                 # 0x000000E0 | 0x00108093
addi x2, x2, 2                 # 0x000000E4 | 0x00210113
addi x3, x3, 3                 # 0x000000E8 | 0x00318193
addi x4, x4, 4                 # 0x000000EC | 0x00420213
addi x5, x5, 5                 # 0x000000F0 | 0x00528293
addi x6, x6, 6                 # 0x000000F4 | 0x00630313
addi x7, x7, 7                 # 0x000000F8 | 0x00738393
addi x8, x8, 8                 # 0x000000FC | 0x00840413
addi x1, x1, 1                 # 0x00000100 | 0x00108093
addi x2, x2, 2                 # 0x00000104 | 0x00210113
addi x3, x3, 3                 # 0x00000108 | 0x00318193
addi x4, x4, 4                 # 0x0000010C | 0x00420213
addi x5, x5, 5                 # 0x00000110 | 0x00528293
addi x6, x6, 6                 # 0x00000114 | 0x00630313
addi x7, x7, 7                 # 0x00000118 | 0x00738393
addi x8, x8, 8                 # 0x0000011C | 0x00840413
ebreak                         # 0x00000120 | 0x00100073
```

## Execution

1. Eight setup instructions initialize `x1` through `x8` to 1 through 8.
2. Execute eight unrolled rounds. Each round increments every register by its own index: `x1 += 1`, ..., `x8 += 8`.
3. Each register finishes at nine times its index, giving `9, 18, 27, 36, 45, 54, 63, 72`.
4. EBREAK follows the 72 ALU instructions, for 73 attempts total.

There is no branch loop in the image. Adjacent instructions use different accumulators; each chain's next update is eight instructions later. This is a workload shape, not a claim about the reference's cycle throughput.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program17-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000009` | Register result |
| `x2` | `0x00000012` | Register result |
| `x3` | `0x0000001B` | Register result |
| `x4` | `0x00000024` | Register result |
| `x5` | `0x0000002D` | Register result |
| `x6` | `0x00000036` | Register result |
| `x7` | `0x0000003F` | Register result |
| `x8` | `0x00000048` | Register result |
| `PC` | `0x00000120` | EBREAK address |
| `Attempts` | 73 | Checkpoints I0–I72 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program17-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. No loads or stores occur. Pipeline timing belongs to the later implementation-specific evaluation.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 17 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 73 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program17 RV32ISingleCycleSystemTest/program-17 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program17&amp;core_fidelity=behavioral" title="RV32I program 17: Independent ALU chains" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program17&amp;core_fidelity=behavioral" class="md-button">Open program 17 reference</a>
