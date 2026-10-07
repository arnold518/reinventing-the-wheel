# Program 13: Out-of-range word load

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Reject an aligned load beginning just beyond the implemented data memory.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-13` |
| Headless selection | `--program 13` |
| Visualizer scenario | `rv32i-program13` |
| Registered test | `RV32ISingleCycleSystemTest/program-13` |
| Instruction image | 3 words, starting at `0x00000000`; last word at `0x00000008` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program13-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | First inaccessible address, 0x00040000 |
| `x2` | Faulting load destination; remains zero |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program13-assembly -->
```asm
lui x1, 0x00040                # 0x00000000 | 0x000400B7
lw x2, 0(x1)                   # 0x00000004 | 0x0000A103
ebreak                         # 0x00000008 | 0x00100073
```

## Execution

1. `lui x1, 0x40` creates address `0x00040000`, the byte immediately after the 256 KiB data memory.
2. LW at `0x04` has valid word alignment but its bytes are outside the implemented range. The result is `LoadAccessFault`, not a misalignment trap.
3. No register load result is written and EBREAK does not execute. PC remains `0x04` after two attempts.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program13-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00040000` | First inaccessible address, 0x00040000 |
| `x2` | `0x00000000` | Faulting load destination; remains zero |
| `PC` | `0x00000004` | Faulting attempt address |
| `Attempts` | 2 | Checkpoints I0–I1 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 6 | `LoadAccessFault`, a project enum code |

<!-- rv32i-program13-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. The 32-bit ISA address space does not imply that all addresses have backing memory.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 13 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 2 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program13 RV32ISingleCycleSystemTest/program-13 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program13&amp;core_fidelity=behavioral" title="RV32I program 13: Out-of-range word load" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program13&amp;core_fidelity=behavioral" class="md-button">Open program 13 reference</a>
