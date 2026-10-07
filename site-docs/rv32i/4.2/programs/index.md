# § 4.2.3. Fixed Programs

[Benchmark index](../index.md) · [Instruction catalogue](../../4.1/instructions/index.md) · [Executable reference](../../4.1/implementation.md)

This catalogue documents **all 22 registered RV32I program fixtures**: sixteen correctness cases and six workloads. Every program has the same sections: **Purpose · Setup · Assembly · Execution · Expected result · Simulation and checkpoints**.

The explanations describe instruction-level behavior. They do not require the later single-cycle or pipeline construction sections.

## Program catalogue

Program numbers are the existing fixture identifiers. There are no omitted programs in the 1–22 range. Together, the programs contain **650 instruction-attempt checkpoints**.

<!-- rv32i-program-catalogue -->
| Program | Category | Attempts |
| --- | --- | ---: |
| [1. Array sum, loop, and stores](program-01.md) | Instruction behavior | 29 |
| [2. Arithmetic, logic, shifts, and comparisons](program-02.md) | Instruction behavior | 35 |
| [3. Signed and unsigned memory accesses](program-03.md) | Instruction behavior | 17 |
| [4. Conditional branches and fallthrough](program-04.md) | Instruction behavior | 24 |
| [5. Direct call, indirect jump, and return](program-05.md) | Instruction behavior | 11 |
| [6. Upper immediates, PC-relative values, and FENCE](program-06.md) | Instruction behavior | 10 |
| [7. Fibonacci sequence](program-07.md) | Instruction behavior | 61 |
| [8. Byte copy and checksum](program-08.md) | Instruction behavior | 42 |
| [9. Illegal instruction and precise stopping](program-09.md) | Traps | 2 |
| [10. Environment-call trap](program-10.md) | Traps | 2 |
| [11. Misaligned word load](program-11.md) | Traps | 2 |
| [12. Misaligned halfword store](program-12.md) | Traps | 3 |
| [13. Out-of-range word load](program-13.md) | Traps | 2 |
| [14. Out-of-range word store](program-14.md) | Traps | 3 |
| [15. Misaligned jump target](program-15.md) | Traps | 2 |
| [16. Instruction-fetch access fault](program-16.md) | Traps | 3 |
| [17. Independent ALU chains](program-17.md) | Workload | 73 |
| [18. Dependent ALU chain](program-18.md) | Workload | 66 |
| [19. Repeated load-use pairs](program-19.md) | Workload | 67 |
| [20. Branch-heavy loop](program-20.md) | Workload | 51 |
| [21. Sixteen-word array sum](program-21.md) | Workload | 84 |
| [22. Word copy and checksum](program-22.md) | Workload | 61 |

## Shared setup

The fixture clears both memories, loads the instruction image and the listed initial data, resets PC and registers to zero, and supplies the clock for the embedded run. Every program page repeats its image size, initial state, data bytes, and register roles.

Instruction and data memories are separate, each with the default 256 KiB capacity. Identical numeric addresses in the two memories do not alias. Unlisted data bytes start at zero. A stack is not allocated by the core; none of these hand-written fixtures uses the standard ABI stack.

All current program fixtures default to zero-wait FF endpoints: reads settle before the next rising edge, and stores commit on that edge. The single-cycle and baseline cores require this timing and contain no pending-request buffers. The advanced pipeline also supports delayed endpoints, tracking stalls and obsolete fetches after redirects while endpoints hold responses until acknowledged. Fixture images are prepared before simulation; CPU reset does not erase either memory.

Each embedded visualizer starts with `RV32I_PROGRAM_ROOT.CORE` at **behavioral** fidelity, with both memories visible. This URL-only selection does not overwrite saved fidelity preferences. Choosing S/B and pressing Apply remains an explicit profile edit.

The scenario menu lists each fixed program once. **Core** selects single-cycle,
baseline pipeline or advanced pipeline; **Prediction** selects one of the five
policies for the advanced core. All 22 programs support all five policies.
Component examples are grouped separately by topic. Existing program and
prediction URLs still select the same registered scenarios and saved profiles.

## Checkpoints and time

- `I0` is the first attempted instruction. Repeated loop instructions get separate checkpoints on every visit.
- A loaded word is not necessarily executed. Conversely, a failed fetch can produce a checkpoint without a successfully fetched word; Program 16 demonstrates this.
- Counts include the terminating EBREAK or fault. A trap is the expected outcome for Programs 9–16.
- Times are absolute simulator units from the behavioral-core recording, not milliseconds or architectural instruction latencies. Changing fidelity or clock scheduling may change them.
- Full checkpoint tables scroll horizontally and vertically. No row sampling or truncation is applied.
- The expected-state tables quote the fixtures' explicit final assertions. They do not imply that unlisted registers are zero. The store tables list all written bytes, including writes of zero.

Programs 17–22 are also used for pipeline workload tests. Their reference pages specify results and dependency patterns, not pipeline cycle counts, forwarding behavior, or stall penalties.

## Running and verification

Stable benchmark IDs are `program-01` through `program-22`. The command-line selector remains `--program N`; chapter renumbering changes neither test names nor simulator scenarios.

```sh
build/rv32i_experiment --core single --program 1 --period auto --allocation-limit-mib 512
python3 tools/benchmarks/run.py rv32i --output build/experiments/program-reference \
  --preset program-regression -- --clock-mode binary-safe --reference-check \
  --allocation-limit-mib 512 --warmups 0 --repetitions 1
```

The second command is an explicit full fixed-program campaign, not part of a documentation build. [Verification](../verification/index.md) distinguishes fixture assertions, oracle lockstep, and fidelity comparisons. The common experiment runner checks the oracle and final memory; the native fixture tests additionally enforce each page's explicit final assertions. [Measurement rules](../execution.md) define attempts versus successful retirements; expected traps are passing correctness cases, not useful throughput samples.

The per-instruction answer sheets remain in the [executable ISA reference](../../4.1/implementation.md#one-step-examples-for-all-40-instructions).
