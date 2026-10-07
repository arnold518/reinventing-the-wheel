# Program 11: Misaligned word load

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Reject a word load from an accessible but non-word-aligned address.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-11` |
| Headless selection | `--program 11` |
| Visualizer scenario | `rv32i-program11` |
| Registered test | `RV32ISingleCycleSystemTest/program-11` |
| Instruction image | 3 words, starting at `0x00000000`; last word at `0x00000008` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program11-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Address 0x101 |
| `x2` | Faulting load destination; remains zero |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program11-assembly -->
```asm
addi x1, x0, 0x101             # 0x00000000 | 0x10100093
lw x2, 0(x1)                   # 0x00000004 | 0x0000A103
ebreak                         # 0x00000008 | 0x00100073
```

## Execution

1. ADDI sets `x1=0x101`, an address inside data memory.
2. LW at `0x04` requests four bytes from that address. Since `0x101 % 4 != 0`, the load produces `LoadAddressMisaligned` before a destination write.
3. PC remains `0x04`, `x2` stays zero, and the following EBREAK does not execute. Two attempts occur.

Zero-filled memory does not turn this into a successful zero-valued load: alignment is checked first.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program11-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000101` | Address 0x101 |
| `x2` | `0x00000000` | Faulting load destination; remains zero |
| `PC` | `0x00000004` | Faulting attempt address |
| `Attempts` | 2 | Checkpoints I0–I1 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 5 | `LoadAddressMisaligned`, a project enum code |

<!-- rv32i-program11-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. This isolates alignment, not capacity: the address range itself is available.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 11 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 2 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program11 RV32ISingleCycleSystemTest/program-11 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program11&amp;core_fidelity=behavioral" title="RV32I program 11: Misaligned word load" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program11&amp;core_fidelity=behavioral" class="md-button">Open program 11 reference</a>
