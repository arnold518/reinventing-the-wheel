# Program 16: Instruction-fetch access fault

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Accept an aligned jump target, then trap when the following fetch cannot access instruction memory.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program16` |
| Registered test | `RV32ISingleCycleSystemTest/program-16` |
| Instruction image | 2 words, starting at `0x00000000`; last word at `0x00000004` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program16-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Aligned target 0x00040000 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program16-assembly -->
```asm
lui x1, 0x00040                # 0x00000000 | 0x000400B7
jalr x0, 0(x1)                 # 0x00000004 | 0x00008067
```

## Execution

1. LUI constructs `0x00040000`, the first address beyond instruction memory.
2. JALR at `0x04` accepts this **aligned** target and updates PC to `0x00040000`. It discards its link through `rd=x0`.
3. The next attempt cannot fetch four instruction bytes at that PC. It records `InstructionAccessFault` and keeps PC at the inaccessible target.

There are three attempts but only two loaded words: the third checkpoint is a failed fetch, not another instruction in the image. This differs from Program 15, where the jump itself is rejected and PC never reaches the target.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program16-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00040000` | Aligned target 0x00040000 |
| `PC` | `0x00040000` | Faulting attempt address |
| `Attempts` | 3 | Checkpoints I0–I2 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 4 | `InstructionAccessFault`, a project enum code |

<!-- rv32i-program16-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. No instruction word is successfully fetched at the final PC. The faulting checkpoint must not be interpreted as a decoded instruction from memory.

## Simulation and checkpoints

All 3 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program16 RV32ISingleCycleSystemTest/program-16 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program16&amp;core_fidelity=behavioral" title="RV32I program 16: Instruction-fetch access fault" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program16&amp;core_fidelity=behavioral" class="md-button">Open program 16 reference</a>
