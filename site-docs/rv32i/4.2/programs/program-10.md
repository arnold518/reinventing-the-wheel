# Program 10: Environment-call trap

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Execute a legal ECALL and verify terminal environment-call handling without executing the following instruction.

This is a negative test: the named trap is the expected outcome, not a failed simulation.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-10` |
| Headless selection | `--program 10` |
| Visualizer scenario | `rv32i-program10` |
| Registered test | `RV32ISingleCycleSystemTest/program-10` |
| Instruction image | 3 words, starting at `0x00000000`; last word at `0x00000008` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program10-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Value written before ECALL |
| `x2` | Must remain zero after ECALL |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program10-assembly -->
```asm
addi x1, x0, 1                 # 0x00000000 | 0x00100093
ecall                          # 0x00000004 | 0x00000073
addi x2, x0, 2                 # 0x00000008 | 0x00200113
```

## Execution

1. ADDI at `0x00` writes `x1=1`.
2. ECALL at `0x04` is legally decoded but requests an environment call. The reference sets `EnvironmentCall` and leaves PC at `0x04`.
3. The ADDI at `0x08` never executes, so `x2` remains zero. There are two attempts and no stores.

Unlike Program 9, this is not an illegal encoding. This environment does not provide a syscall service or a resumable handler; ECALL is a terminal trap.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program10-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x1` | `0x00000001` | Value written before ECALL |
| `x2` | `0x00000000` | Must remain zero after ECALL |
| `PC` | `0x00000004` | Faulting attempt address |
| `Attempts` | 2 | Checkpoints I0–I1 |
| `HALTED` | 0 | No EBREAK halt |
| `TRAPPED` | 1 | Expected terminal exception |
| `TRAP_CAUSE` | 2 | `EnvironmentCall`, a project enum code |

<!-- rv32i-program10-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

No data writes occur; the initial data image is unchanged. HALTED remains 0 and TRAPPED becomes 1; ECALL is not the project's EBREAK halt.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 10 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 2 instruction attempts are included, with absolute simulator time and checkpoint number. The terminal fault is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program10 RV32ISingleCycleSystemTest/program-10 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program10&amp;core_fidelity=behavioral" title="RV32I program 10: Environment-call trap" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program10&amp;core_fidelity=behavioral" class="md-button">Open program 10 reference</a>
