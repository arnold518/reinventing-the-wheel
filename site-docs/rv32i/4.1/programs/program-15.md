# Program 15: Misaligned jump target

[Program catalogue](index.md) · [Instruction reference](../instructions/index.md) · [Executable reference](../implementation.md)

## Purpose

Reject JALR's target alignment on the jump instruction itself.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Visualizer scenario | `rv32i-program15` |
| Registered test | `RV32ISingleCycleSystemTest/program-15` |
| Instruction image | 2 words, starting at `0x00000000`; last word at `0x00000004` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program15-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Candidate target address 2 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program15-assembly -->
```asm
addi x1, x0, 2                 # 0x00000000 | 0x00200093
jalr x0, 0(x1)                 # 0x00000004 | 0x00008067
```

## Execution

1. ADDI sets the candidate target to byte address 2.
2. JALR at `0x04` adds zero and clears target bit 0. Address 2 is still 2 because bit 1, not bit 0, is set.
3. The target violates four-byte instruction alignment. `InstructionAddressMisaligned` is recorded on JALR, PC remains `0x04`, and no fetch at address 2 occurs.

The destination is `x0`, so this fixture does not test preservation of a nonzero link register. The separate instruction boundary examples cover that case.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program15-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000002` | Candidate target address 2 |
| `PC` | `0x00000004` | Faulting attempt address |
| `Attempts` | 2 | Checkpoints I0–I1 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 3 | `InstructionAddressMisaligned`, a project enum code |

<!-- rv32i-program15-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. Two attempts occur. Compare Program 16: an aligned inaccessible target is accepted before its next fetch faults.

## Simulation and checkpoints

All 2 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program15 RV32ISingleCycleSystemTest/program-15 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program15&amp;core_fidelity=behavioral" title="RV32I program 15: Misaligned jump target" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program15&amp;core_fidelity=behavioral" class="md-button">Open program 15 reference</a>
