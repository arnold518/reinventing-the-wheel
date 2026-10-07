# Program 22: Word copy and checksum

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Provide a mixed-memory workload that copies eight words while accumulating their checksum.

This is one of the six workload fixtures. This page specifies its instructions and architectural result; pipeline cycles, stalls, and throughput are implementation-specific and are not established by this reference run.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-22` |
| Headless selection | `--program 22` |
| Visualizer scenario | `rv32i-program22` |
| Registered test | `RV32ISingleCycleSystemTest/program-22` |
| Instruction image | 12 words, starting at `0x00000000`; last word at `0x0000002C` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program22-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000100` | `10 00 00 00` |
| `0x00000104` | `11 00 00 00` |
| `0x00000108` | `12 00 00 00` |
| `0x0000010C` | `13 00 00 00` |
| `0x00000110` | `14 00 00 00` |
| `0x00000114` | `15 00 00 00` |
| `0x00000118` | `16 00 00 00` |
| `0x0000011C` | `17 00 00 00` |

The table lists the complete preloaded data image, not instruction bytes. Data values use little-endian byte order.

| Register | Role established by the program |
| --- | --- |
| `x1` | Next source word address |
| `x2` | Next destination word address |
| `x3` | Remaining word count |
| `x4` | Checksum |
| `x5` | Loaded word |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program22-assembly -->
```asm
addi x1, x0, 0x100             # 0x00000000 | 0x10000093
addi x2, x0, 0x180             # 0x00000004 | 0x18000113
addi x3, x0, 8                 # 0x00000008 | 0x00800193
addi x4, x0, 0                 # 0x0000000C | 0x00000213
loop:
lw x5, 0(x1)                   # 0x00000010 | 0x0000A283
sw x5, 0(x2)                   # 0x00000014 | 0x00512023
add x4, x4, x5                 # 0x00000018 | 0x00520233
addi x1, x1, 4                 # 0x0000001C | 0x00408093
addi x2, x2, 4                 # 0x00000020 | 0x00410113
addi x3, x3, -1                # 0x00000024 | 0xFFF18193
bne x3, x0, loop               # 0x00000028 | 0xFE0194E3
ebreak                         # 0x0000002C | 0x00100073
```

## Execution

1. Initialize source `0x100`, destination `0x180`, count eight, and checksum zero. Input words are `0x10` through `0x17` (16 through 23).
2. Each pass loads one word, stores it to the destination, adds it to the checksum, advances both pointers by four bytes, decrements the count, and conditionally repeats.
3. Eight copied words occupy `0x180`–`0x19F`. The checksum is `16 + ... + 23 = 156 = 0x9C`.
4. Final pointers are `0x120` and `0x1A0`; the count is zero. EBREAK makes `4 + 8 × 7 + 1 = 61` attempts.

The checksum remains in `x4`; the program does not store it after the copied array. Source bytes remain unchanged.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program22-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000120` | Next source word address |
| `x2` | `0x000001A0` | Next destination word address |
| `x3` | `0x00000000` | Remaining word count |
| `x4` | `0x0000009C` | Checksum |
| `x5` | `0x00000017` | Loaded word |
| `PC` | `0x0000002C` | EBREAK address |
| `Attempts` | 61 | Checkpoints I0–I60 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program22-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000180` | `10 00 00 00` |
| `0x00000184` | `11 00 00 00` |
| `0x00000188` | `12 00 00 00` |
| `0x0000018C` | `13 00 00 00` |
| `0x00000190` | `14 00 00 00` |
| `0x00000194` | `15 00 00 00` |
| `0x00000198` | `16 00 00 00` |
| `0x0000019C` | `17 00 00 00` |

All data bytes not listed as written retain their initial values. This differs from Program 8's byte copy: pointers advance by four, and there is no separate checksum store.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 22 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 61 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program22 RV32ISingleCycleSystemTest/program-22 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program22&amp;core_fidelity=behavioral" title="RV32I program 22: Word copy and checksum" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program22&amp;core_fidelity=behavioral" class="md-button">Open program 22 reference</a>
