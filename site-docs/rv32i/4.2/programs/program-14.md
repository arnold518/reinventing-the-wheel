# Program 14: Out-of-range word store

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Reject an aligned store beyond data memory without changing any bytes.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-14` |
| Headless selection | `--program 14` |
| Visualizer scenario | `rv32i-program14` |
| Registered test | `RV32ISingleCycleSystemTest/program-14` |
| Instruction image | 4 words, starting at `0x00000000`; last word at `0x0000000C` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program14-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | First inaccessible address, 0x00040000 |
| `x2` | Store source 0x7B (123) |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program14-assembly -->
```asm
lui x1, 0x00040                # 0x00000000 | 0x000400B7
addi x2, x0, 123               # 0x00000004 | 0x07B00113
sw x2, 0(x1)                   # 0x00000008 | 0x0020A023
ebreak                         # 0x0000000C | 0x00100073
```

## Execution

1. LUI constructs `0x00040000`; ADDI sets the intended store value to `0x7B`.
2. SW at `0x08` is aligned but cannot access its four-byte range, so it produces `StoreAccessFault`.
3. Memory is unchanged, the earlier register values remain, and PC stays at `0x08`. The following EBREAK never executes; there are three attempts.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program14-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00040000` | First inaccessible address, 0x00040000 |
| `x2` | `0x0000007B` | Store source 0x7B (123) |
| `PC` | `0x00000008` | Faulting attempt address |
| `Attempts` | 3 | Checkpoints I0–I2 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 8 | `StoreAccessFault`, a project enum code |

<!-- rv32i-program14-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. An out-of-range store is not silently discarded as a successful instruction: it terminates with a trap.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 14 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 3 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program14 RV32ISingleCycleSystemTest/program-14 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program14&amp;core_fidelity=behavioral" title="RV32I program 14: Out-of-range word store" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program14&amp;core_fidelity=behavioral" class="md-button">Open program 14 reference</a>
