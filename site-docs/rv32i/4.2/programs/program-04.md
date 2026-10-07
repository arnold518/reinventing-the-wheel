# Program 4: Conditional branches and fallthrough

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Exercise all six RV32I conditional branch forms, including signed/unsigned interpretation and a not-taken branch.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-04` |
| Headless selection | `--program 4` |
| Visualizer scenario | `rv32i-program4` |
| Registered test | `RV32ISingleCycleSystemTest/program-04` |
| Instruction image | 31 words, starting at `0x00000000`; last word at `0x00000078` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program4-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1, x2` | Equality operands, initially 5 and 5; x2 later becomes 6 |
| `x3` | Bit pattern for -1 |
| `x4` | Comparison operand 1 |
| `x10` | Score |
| `x11` | Output address 0x120 |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program4-assembly -->
```asm
addi x10, x0, 0                # 0x00000000 | 0x00000513
addi x1, x0, 5                 # 0x00000004 | 0x00500093
addi x2, x0, 5                 # 0x00000008 | 0x00500113
beq x1, x2, L_14               # 0x0000000C | 0x00208463
addi x10, x10, 100             # 0x00000010 | 0x06450513
L_14:
addi x10, x10, 1               # 0x00000014 | 0x00150513
addi x2, x0, 6                 # 0x00000018 | 0x00600113
bne x1, x2, L_24               # 0x0000001C | 0x00209463
addi x10, x10, 100             # 0x00000020 | 0x06450513
L_24:
addi x10, x10, 1               # 0x00000024 | 0x00150513
addi x3, x0, -1                # 0x00000028 | 0xFFF00193
addi x4, x0, 1                 # 0x0000002C | 0x00100213
blt x3, x4, L_38               # 0x00000030 | 0x0041C463
addi x10, x10, 100             # 0x00000034 | 0x06450513
L_38:
addi x10, x10, 1               # 0x00000038 | 0x00150513
bge x4, x3, L_44               # 0x0000003C | 0x00325463
addi x10, x10, 100             # 0x00000040 | 0x06450513
L_44:
addi x10, x10, 1               # 0x00000044 | 0x00150513
bltu x4, x3, L_50              # 0x00000048 | 0x00326463
addi x10, x10, 100             # 0x0000004C | 0x06450513
L_50:
addi x10, x10, 1               # 0x00000050 | 0x00150513
bgeu x3, x4, L_5C              # 0x00000054 | 0x0041F463
addi x10, x10, 100             # 0x00000058 | 0x06450513
L_5C:
addi x10, x10, 1               # 0x0000005C | 0x00150513
beq x1, x4, L_6C               # 0x00000060 | 0x00408663
addi x10, x10, 1               # 0x00000064 | 0x00150513
jal x0, L_70                   # 0x00000068 | 0x0080006F
L_6C:
addi x10, x10, 100             # 0x0000006C | 0x06450513
L_70:
addi x11, x0, 0x120            # 0x00000070 | 0x12000593
sw x10, 0(x11)                 # 0x00000074 | 0x00A5A023
ebreak                         # 0x00000078 | 0x00100073
```

## Execution

1. Start the score at zero. BEQ for `5 == 5` and BNE for `5 != 6` each take a branch over an incorrect-path `+100` instruction, then add one.
2. BLT tests signed `-1 < 1`; BGE tests signed `1 >= -1`. Both take their branches and add one.
3. BLTU tests unsigned `1 < 0xFFFFFFFF`; BGEU tests unsigned `0xFFFFFFFF >= 1`. Both take their branches and add one.
4. The final BEQ compares 5 with 1 and must **not** branch. Its fallthrough adds the seventh point. JAL then skips the incorrect-path `+100` before storing the score.
5. Store 7 at `0x120` and halt. Seven wrong-path additions are skipped: 31 loaded words produce 24 attempts.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program4-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x10` | `0x00000007` | Score |
| `x11` | `0x00000120` | Output address 0x120 |
| `PC` | `0x00000078` | EBREAK address |
| `Attempts` | 24 | Checkpoints I0–I23 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program4-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `07 00 00 00` |

All data bytes not listed as written retain their initial values. A score other than 7 exposes a comparison, branch-target, or fallthrough error. The six initial taken branches and the final not-taken BEQ are distinct cases.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 4 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 24 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program4 RV32ISingleCycleSystemTest/program-04 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program4&amp;core_fidelity=behavioral" title="RV32I program 4: Conditional branches and fallthrough" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program4&amp;core_fidelity=behavioral" class="md-button">Open program 4 reference</a>
