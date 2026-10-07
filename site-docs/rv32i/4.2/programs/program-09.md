# Program 9: Illegal instruction and precise stopping

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Reject an invalid word while preserving earlier effects and preventing later instructions from executing.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-09` |
| Headless selection | `--program 9` |
| Visualizer scenario | `rv32i-program9` |
| Registered test | `RV32ISingleCycleSystemTest/program-09` |
| Instruction image | 3 words, starting at `0x00000000`; last word at `0x00000008` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program9-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Effect completed before the fault |
| `x2` | Assignment after the fault; must remain zero |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program9-assembly -->
```asm
addi x1, x0, 1                 # 0x00000000 | 0x00100093
.word 0xFFFFFFFF               # 0x00000004 | 0xFFFFFFFF
addi x2, x0, 2                 # 0x00000008 | 0x00200113
```

`.word` emits raw bits; it is an assembler directive, not an RV32I instruction.

## Execution

| Checkpoint | Address | Effect |
| --- | --- | --- |
| I0 | `0x00` | ADDI writes `x1=1` and advances PC to `0x04`. |
| I1 | `0x04` | Fetch succeeds, but decoding `0xFFFFFFFF` rejects it. The reference records `IllegalInstruction`; PC remains `0x04`. |
| None | `0x08` | This ADDI is loaded but never executed. It produces no checkpoint and leaves `x2=0`. |

The fault does not undo the completed write to `x1`. It also does not advance PC or permit the following write to `x2`. Only two attempts occur even though the image contains three words.

Unlike EBREAK, this stop sets `TRAPPED=1` and leaves `HALTED=0`. The current environment stops at the trap; it does not transfer to a handler or resume at `0x08`.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program9-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000001` | Effect completed before the fault |
| `x2` | `0x00000000` | Assignment after the fault; must remain zero |
| `PC` | `0x00000004` | Faulting attempt address |
| `Attempts` | 2 | Checkpoints I0–I1 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 1 | `IllegalInstruction`, a project enum code |

<!-- rv32i-program9-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. The trap is the intended outcome. The .word directive emits raw bits; it is not a decoded RV32I instruction.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 9 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 2 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program9 RV32ISingleCycleSystemTest/program-09 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program9&amp;core_fidelity=behavioral" title="RV32I program 9: Illegal instruction and precise stopping" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program9&amp;core_fidelity=behavioral" class="md-button">Open program 9 reference</a>
