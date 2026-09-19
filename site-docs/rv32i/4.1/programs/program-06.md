# Program 6: Upper immediates, PC-relative values, and FENCE

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Construct a full-width constant, read PC-relative values with AUIPC, and execute a no-effect FENCE.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program6` |
| Registered test | `RV32ISingleCycleSystemTest/program-06` |
| Instruction image | 10 words, starting at `0x00000000`; last word at `0x00000024` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program6-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Constructed constant 0x12345678 |
| `x2` | PC of AUIPC at 0x08 |
| `x3` | PC of AUIPC at 0x0C plus 0x1000 |
| `x4` | Output base at 0x120 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program6-assembly -->
```asm
lui x1, 0x12345                # 0x00000000 | 0x123450B7
addi x1, x1, 0x678             # 0x00000004 | 0x67808093
auipc x2, 0x00000              # 0x00000008 | 0x00000117
auipc x3, 0x00001              # 0x0000000C | 0x00001197
fence 0, 0                     # 0x00000010 | 0x0000000F
addi x4, x0, 0x120             # 0x00000014 | 0x12000213
sw x1, 0(x4)                   # 0x00000018 | 0x00122023
sw x2, 4(x4)                   # 0x0000001C | 0x00222223
sw x3, 8(x4)                   # 0x00000020 | 0x00322423
ebreak                         # 0x00000024 | 0x00100073
```

## Execution

1. LUI places `0x12345` in the upper twenty bits. ADDI supplies the low `0x678`, forming `0x12345678`.
2. AUIPC at `0x08` with upper immediate zero writes `0x08` to `x2`. AUIPC at `0x0C` with upper immediate one writes `0x100C` to `x3`. Each uses its own instruction address, not PC+4.
3. The fixture's exact fence word is `0x0000000F`, shown as `fence 0, 0`: both predecessor and successor sets are empty. It changes no register or memory data in this serial reference.
4. Store the three results at `0x120`, `0x124`, and `0x128`, then halt after ten attempts.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program6-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x12345678` | Constructed constant 0x12345678 |
| `x2` | `0x00000008` | PC of AUIPC at 0x08 |
| `x3` | `0x0000100C` | PC of AUIPC at 0x0C plus 0x1000 |
| `PC` | `0x00000024` | EBREAK address |
| `Attempts` | 10 | Checkpoints I0–I9 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program6-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `78 56 34 12` |
| `0x00000124` | `08 00 00 00` |
| `0x00000128` | `0C 10 00 00` |

All data bytes not listed as written retain their initial values. The FENCE case is not a cache flush or a proof of multicore ordering. The assembly lists unshifted U-format operands; the resulting register values are shifted by twelve bits.

## Simulation and checkpoints

All 10 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program6 RV32ISingleCycleSystemTest/program-06 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program6&amp;core_fidelity=behavioral" title="RV32I program 6: Upper immediates, PC-relative values, and FENCE" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program6&amp;core_fidelity=behavioral" class="md-button">Open program 6 reference</a>
