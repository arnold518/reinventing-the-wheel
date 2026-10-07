# Program 21: Sixteen-word array sum

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Provide a scalar array-sum workload combining a load-use dependency, address updates, a counter, and loop branches.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-21` |
| Headless selection | `--program 21` |
| Visualizer scenario | `rv32i-program21` |
| Registered test | `RV32ISingleCycleSystemTest/program-21` |
| Instruction image | 9 words, starting at `0x00000000`; last word at `0x00000020` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program21-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `01 00 00 00` |
| `0x00000104` | `02 00 00 00` |
| `0x00000108` | `03 00 00 00` |
| `0x0000010C` | `04 00 00 00` |
| `0x00000110` | `05 00 00 00` |
| `0x00000114` | `06 00 00 00` |
| `0x00000118` | `07 00 00 00` |
| `0x0000011C` | `08 00 00 00` |
| `0x00000120` | `09 00 00 00` |
| `0x00000124` | `0A 00 00 00` |
| `0x00000128` | `0B 00 00 00` |
| `0x0000012C` | `0C 00 00 00` |
| `0x00000130` | `0D 00 00 00` |
| `0x00000134` | `0E 00 00 00` |
| `0x00000138` | `0F 00 00 00` |
| `0x0000013C` | `10 00 00 00` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Next input word address |
| `x2` | Remaining word count |
| `x3` | Sum |
| `x4` | Loaded word |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program21-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
addi x2, x0, 16                # 0x00000004 | 0x01000113
addi x3, x0, 0                 # 0x00000008 | 0x00000193
loop:
lw x4, 0(x1)                   # 0x0000000C | 0x0000A203
add x3, x3, x4                 # 0x00000010 | 0x004181B3
addi x1, x1, 4                 # 0x00000014 | 0x00408093
addi x2, x2, -1                # 0x00000018 | 0xFFF10113
bne x2, x0, loop               # 0x0000001C | 0xFE0118E3
ebreak                         # 0x00000020 | 0x00100073
```

## Execution

1. Initialize source `0x100`, count sixteen, and sum zero. Input words are the integers 1 through 16.
2. Each pass loads one word, immediately adds it to the sum, advances the source by four bytes, decrements the count, and conditionally repeats.
3. The sum is `16 × 17 / 2 = 136`; the last loaded value is 16 and the source ends at `0x140`.
4. EBREAK ends `3 + 16 × 5 + 1 = 84` attempts. The branch is taken fifteen times.

Unlike Program 1, this fixture has no result stores. Its sum stays in `x3`, and the input memory is unchanged.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program21-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000140` | Next input word address |
| `x2` | `0x00000000` | Remaining word count |
| `x3` | `0x00000088` | Sum |
| `x4` | `0x00000010` | Loaded word |
| `PC` | `0x00000020` | EBREAK address |
| `Attempts` | 84 | Checkpoints I0–I83 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program21-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. The result is observed through architectural registers, not through an output buffer.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 21 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 84 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program21 RV32ISingleCycleSystemTest/program-21 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program21&amp;core_fidelity=behavioral" title="RV32I program 21: Sixteen-word array sum" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program21&amp;core_fidelity=behavioral" class="md-button">Open program 21 reference</a>
