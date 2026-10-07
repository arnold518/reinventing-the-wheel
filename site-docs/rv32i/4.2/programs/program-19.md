# Program 19: Repeated load-use pairs

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Provide an unrolled sequence where each arithmetic instruction immediately consumes a loaded value.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-19` |
| Headless selection | `--program 19` |
| Visualizer scenario | `rv32i-program19` |
| Registered test | `RV32ISingleCycleSystemTest/program-19` |
| Instruction image | 67 words, starting at `0x00000000`; last word at `0x00000108` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program19-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `07 00 00 00` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Fixed source address 0x100 |
| `x2` | Loaded word, always 7 |
| `x3` | Accumulator |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image. The repeated blocks are fully expanded; there are no omitted instructions or runtime loop directives.

<!-- rv32i-program19-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
addi x3, x0, 0                 # 0x00000004 | 0x00000193
lw x2, 0(x1)                   # 0x00000008 | 0x0000A103
add x3, x3, x2                 # 0x0000000C | 0x002181B3
lw x2, 0(x1)                   # 0x00000010 | 0x0000A103
add x3, x3, x2                 # 0x00000014 | 0x002181B3
lw x2, 0(x1)                   # 0x00000018 | 0x0000A103
add x3, x3, x2                 # 0x0000001C | 0x002181B3
lw x2, 0(x1)                   # 0x00000020 | 0x0000A103
add x3, x3, x2                 # 0x00000024 | 0x002181B3
lw x2, 0(x1)                   # 0x00000028 | 0x0000A103
add x3, x3, x2                 # 0x0000002C | 0x002181B3
lw x2, 0(x1)                   # 0x00000030 | 0x0000A103
add x3, x3, x2                 # 0x00000034 | 0x002181B3
lw x2, 0(x1)                   # 0x00000038 | 0x0000A103
add x3, x3, x2                 # 0x0000003C | 0x002181B3
lw x2, 0(x1)                   # 0x00000040 | 0x0000A103
add x3, x3, x2                 # 0x00000044 | 0x002181B3
lw x2, 0(x1)                   # 0x00000048 | 0x0000A103
add x3, x3, x2                 # 0x0000004C | 0x002181B3
lw x2, 0(x1)                   # 0x00000050 | 0x0000A103
add x3, x3, x2                 # 0x00000054 | 0x002181B3
lw x2, 0(x1)                   # 0x00000058 | 0x0000A103
add x3, x3, x2                 # 0x0000005C | 0x002181B3
lw x2, 0(x1)                   # 0x00000060 | 0x0000A103
add x3, x3, x2                 # 0x00000064 | 0x002181B3
lw x2, 0(x1)                   # 0x00000068 | 0x0000A103
add x3, x3, x2                 # 0x0000006C | 0x002181B3
lw x2, 0(x1)                   # 0x00000070 | 0x0000A103
add x3, x3, x2                 # 0x00000074 | 0x002181B3
lw x2, 0(x1)                   # 0x00000078 | 0x0000A103
add x3, x3, x2                 # 0x0000007C | 0x002181B3
lw x2, 0(x1)                   # 0x00000080 | 0x0000A103
add x3, x3, x2                 # 0x00000084 | 0x002181B3
lw x2, 0(x1)                   # 0x00000088 | 0x0000A103
add x3, x3, x2                 # 0x0000008C | 0x002181B3
lw x2, 0(x1)                   # 0x00000090 | 0x0000A103
add x3, x3, x2                 # 0x00000094 | 0x002181B3
lw x2, 0(x1)                   # 0x00000098 | 0x0000A103
add x3, x3, x2                 # 0x0000009C | 0x002181B3
lw x2, 0(x1)                   # 0x000000A0 | 0x0000A103
add x3, x3, x2                 # 0x000000A4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000A8 | 0x0000A103
add x3, x3, x2                 # 0x000000AC | 0x002181B3
lw x2, 0(x1)                   # 0x000000B0 | 0x0000A103
add x3, x3, x2                 # 0x000000B4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000B8 | 0x0000A103
add x3, x3, x2                 # 0x000000BC | 0x002181B3
lw x2, 0(x1)                   # 0x000000C0 | 0x0000A103
add x3, x3, x2                 # 0x000000C4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000C8 | 0x0000A103
add x3, x3, x2                 # 0x000000CC | 0x002181B3
lw x2, 0(x1)                   # 0x000000D0 | 0x0000A103
add x3, x3, x2                 # 0x000000D4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000D8 | 0x0000A103
add x3, x3, x2                 # 0x000000DC | 0x002181B3
lw x2, 0(x1)                   # 0x000000E0 | 0x0000A103
add x3, x3, x2                 # 0x000000E4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000E8 | 0x0000A103
add x3, x3, x2                 # 0x000000EC | 0x002181B3
lw x2, 0(x1)                   # 0x000000F0 | 0x0000A103
add x3, x3, x2                 # 0x000000F4 | 0x002181B3
lw x2, 0(x1)                   # 0x000000F8 | 0x0000A103
add x3, x3, x2                 # 0x000000FC | 0x002181B3
lw x2, 0(x1)                   # 0x00000100 | 0x0000A103
add x3, x3, x2                 # 0x00000104 | 0x002181B3
ebreak                         # 0x00000108 | 0x00100073
```

## Execution

1. Set the pointer to `0x100` and the accumulator to zero. Data word `0x00000007` is preloaded at that address.
2. Execute 32 unrolled LW/ADD pairs. Each LW rereads the **same** address; its following ADD adds 7 to `x3`.
3. The total is `32 × 7 = 224`, with `2 + 32 × 2 + 1 = 67` attempts including EBREAK.

There are no branch instructions or pointer updates, so this isolates the load-to-consumer pattern from loop control. Instruction and data addresses can overlap numerically because the fixture uses separate memories.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program19-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000100` | Fixed source address 0x100 |
| `x2` | `0x00000007` | Loaded word, always 7 |
| `x3` | `0x000000E0` | Accumulator |
| `PC` | `0x00000108` | EBREAK address |
| `Attempts` | 67 | Checkpoints I0–I66 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program19-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. Data memory is read but never written. The page documents the workload, not a fixed number of load-use stall cycles.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 19 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 67 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program19 RV32ISingleCycleSystemTest/program-19 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program19&amp;core_fidelity=behavioral" title="RV32I program 19: Repeated load-use pairs" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program19&amp;core_fidelity=behavioral" class="md-button">Open program 19 reference</a>
