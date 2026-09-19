# Program 12: Misaligned halfword store

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Reject an unaligned halfword store without writing either of its bytes.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program12` |
| Registered test | `RV32ISingleCycleSystemTest/program-12` |
| Instruction image | 4 words, starting at `0x00000000`; last word at `0x0000000C` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program12-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Odd address 0x101 |
| `x2` | Store source 0x123 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program12-assembly -->
```asm
addi x1, x0, 0x101             # 0x00000000 | 0x10100093
addi x2, x0, 0x123             # 0x00000004 | 0x12300113
sh x2, 0(x1)                   # 0x00000008 | 0x00209023
ebreak                         # 0x0000000C | 0x00100073
```

## Execution

1. Initialize the address to `0x101` and source value to `0x123`.
2. SH at `0x08` requires a two-byte-aligned address. The odd address causes `StoreAddressMisaligned`.
3. Neither byte is written. Earlier register writes remain, PC stays at `0x08`, and the EBREAK at `0x0C` is not executed.

Three attempts occur. A rejected store must not leave a partially updated memory value.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program12-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000101` | Odd address 0x101 |
| `x2` | `0x00000123` | Store source 0x123 |
| `PC` | `0x00000008` | Faulting attempt address |
| `Attempts` | 3 | Checkpoints I0–I2 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 7 | `StoreAddressMisaligned`, a project enum code |

<!-- rv32i-program12-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. The store source remains 0x123, but no data bytes receive it.

## Simulation and checkpoints

All 3 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program12 RV32ISingleCycleSystemTest/program-12 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program12&amp;core_fidelity=behavioral" title="RV32I program 12: Misaligned halfword store" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program12&amp;core_fidelity=behavioral" class="md-button">Open program 12 reference</a>
