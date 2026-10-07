# Program 5: Direct call, indirect jump, and return

[Program catalogue](index.md) · [Instruction reference](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

## Purpose

Execute nested calls using separate link registers, return to both callers, and store the result.

This is an instruction-behavior correctness fixture.

## Setup

| Property | Value |
| --- | --- |
| Benchmark workload ID | `program-05` |
| Headless selection | `--program 5` |
| Visualizer scenario | `rv32i-program5` |
| Registered test | `RV32ISingleCycleSystemTest/program-05` |
| Instruction image | 11 words, starting at `0x00000000`; last word at `0x00000028` |
| Initial state | PC = `0x00000000`; all registers zero; neither halted nor trapped |
| Memories | Separate instruction and data memories, 256 KiB each; unlisted data bytes zero |

<!-- rv32i-program5-initial-data -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| None | — |

The table lists the complete preloaded data image, not instruction bytes. No data image is required.

| Register | Role established by the program |
| --- | --- |
| `x1` | Output address 0x120, not a return address here |
| `x5` | Outer return address |
| `x6` | Inner return address |
| `x10` | Computed value |

The [shared setup](index.md#shared-setup) describes reset, loading, and the behavioral-core selection. These hand-written programs do not use the standard ABI stack.

## Assembly

Every instruction word is shown, including words that a trap or branch may prevent from executing. Address and word comments identify the exact fixture image.

<!-- rv32i-program5-assembly -->
```asm
addi x1, x0, 0x120             # 0x00000000 | 0x12000093
jal x5, helper_a               # 0x00000004 | 0x00C002EF
sw x10, 0(x1)                  # 0x00000008 | 0x00A0A023
jal x0, done                   # 0x0000000C | 0x01C0006F
helper_a:
addi x10, x0, 7                # 0x00000010 | 0x00700513
jal x6, helper_b               # 0x00000014 | 0x00C0036F
addi x10, x10, 1               # 0x00000018 | 0x00150513
jalr x0, 0(x5)                 # 0x0000001C | 0x00028067
helper_b:
addi x10, x10, 35              # 0x00000020 | 0x02350513
jalr x0, 0(x6)                 # 0x00000024 | 0x00030067
done:
ebreak                         # 0x00000028 | 0x00100073
```

## Execution

| Checkpoints | Executed addresses | Effect |
| --- | --- | --- |
| I0–I1 | `0x00 → 0x04` | Set the output pointer. JAL saves `0x08` in `x5` and enters `helper_a` at `0x10`. |
| I2–I3 | `0x10 → 0x14` | Set `x10=7`. The nested JAL saves `0x18` in `x6` and enters `helper_b` at `0x20`. |
| I4–I5 | `0x20 → 0x24` | Add 35, giving `x10=42`. JALR reads `x6` and returns to `0x18`. |
| I6–I7 | `0x18 → 0x1C` | Add one, giving `x10=43`. JALR reads the preserved `x5` and returns to `0x08`. |
| I8–I9 | `0x08 → 0x0C` | Store 43 at `0x120`, then jump to `done` at `0x28`. |
| I10 | `0x28` | EBREAK halts. |

Each call saves its own **PC+4**, not its target address. The inner call writes `x6`, leaving the outer link in `x5` intact. Both returns use `rd=x0`, so they redirect PC without replacing either saved link.

The final `jal x0, done` also discards its link and avoids falling through into `helper_a` again. All 11 loaded words execute once, but not in increasing address order.

## Expected result

These are the fixture's explicit final-state assertions. Unlisted registers are **not** implicitly zero; their values follow the assembly. The store table enumerates every written byte, including zero-valued writes.

<!-- rv32i-program5-result -->
| Item | Expected value | Meaning |
| --- | --- | --- |
| `x5` | `0x00000008` | Outer return address |
| `x6` | `0x00000018` | Inner return address |
| `x10` | `0x0000002B` | Computed value |
| `PC` | `0x00000028` | EBREAK address |
| `Attempts` | 11 | Checkpoints I0–I10 |
| `HALTED` | 1 | Project halt |
| `TRAPPED` | 0 | No exception |
| `TRAP_CAUSE` | 0 | `None`, a project enum code |

<!-- rv32i-program5-stores -->
| Start address | Bytes, in increasing address order |
| --- | --- |
| `0x00000120` | `2B 00 00 00` |

All data bytes not listed as written retain their initial values. This is not the standard ABI call convention. Distinct link registers preserve both returns without stack pushes, pops, or frames.

## Simulation and checkpoints

For a headless functional check, run `build/rv32i_experiment --core single --program 5 --period 4000 --max-cycles 4096 --allocation-limit-mib 512`. Select `baseline` or `advanced` for the same fixture on another core; add `--fidelity reference` for the structural-datapath reference profile. The explicit period is a scheduling choice, not a performance result. Use [the campaign procedure](../campaigns.md) for clock-certified measurements.

All 11 instruction attempts are included, with absolute simulator time and checkpoint number. The terminating EBREAK is included. The table is scrollable in both directions. [Observation conventions](index.md#checkpoints-and-time) distinguish these attempts from loaded word addresses and hardware cycles.

<!-- simulator-program: rv32i-program5 RV32ISingleCycleSystemTest/program-05 -->

<div class="simulator-embed">
  <iframe src="../../../../simulator/?scenario=rv32i-program5&amp;core_fidelity=behavioral" title="RV32I program 5: Direct call, indirect jump, and return" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../../simulator/?scenario=rv32i-program5&amp;core_fidelity=behavioral" class="md-button">Open program 5 reference</a>
