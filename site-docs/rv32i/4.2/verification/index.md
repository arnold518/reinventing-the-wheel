# § 4.2.6. Verification Reference

[Benchmark overview](../index.md) · [Campaigns](../campaigns.md)

## Evidence layers

| Layer | Question answered | Reference |
| --- | --- | --- |
| ISA answer sheet | Are encodings and single-instruction state changes correct? | [§ 4.1.4](../../4.1/implementation.md) and [workload tests](workloads.md) |
| Fixed program assertions | Does each fixture halt/trap with its specified registers and memory? | [22 program pages](../programs/index.md) |
| Generated host expectations | Does the emitted program perform the intended algorithm? | [Workload tests](workloads.md) |
| Instruction lockstep | Does the executing core match the oracle on each instruction attempt? | [Workload tests](workloads.md) |
| Profile equivalence | Do compact and reference implementations agree, including cycle behavior? | [Equivalence tests](equivalence.md) |
| Clock analysis | Is the chosen period sufficient under declared timing/state contracts? | [Timing tests](timing.md) |
| Harness and artifacts | Are selections, budgets, counters, and recorded results trustworthy? | [Infrastructure tests](infrastructure.md) |
| External self-check | Does the executed image's independent verifier pass? | [Compiled workloads](../compiled/index.md) |

These are complementary, not interchangeable. The behavioral single-cycle implementation shares instruction-oracle logic, so an oracle comparison alone is not independent proof of that implementation. Explicit fixture expectations, generated host results, lower-level component checks, and external self-checks add different evidence.

The 46 differential **pairs** are not 46 programs, the 37-item native **catalogue** is not the full CTest inventory, and 650 fixed-program **checkpoints** are not 650 pipeline cycles.

CTest registrations cover bounded campaign and recovery checks; larger
campaigns have separate artifacts and validation scopes. A test registration
is not a benchmark case or an external program. Dated test and campaign
outcomes are reported in [§ 4.7](../../experiments.md).

## Scope

This section documents tests of the benchmark system and the core behavior it relies on. Component pin/truth-table tests remain with [building blocks](../../4.3/index.md), [single-cycle stages](../../4.4/index.md), [baseline stages](../../4.5/index.md), and [advanced pipeline components](../../4.6/index.md). A successful benchmark run does not replace their structural/behavioral contract tests.

The detailed pages identify test inputs, checks, and limitations. The source test remains authoritative if counts evolve. Large external campaigns are opt-in; normal CTest does not download Embench.

## Focused execution

```sh
ctest --test-dir build --output-on-failure -j1 \
  -R '^(BenchmarkModelTest|BenchmarkCampaignCLITest|RV32IBenchmarkTest|RV32IBenchmarkCLITest|RV32IExperimentTest|RV32IExperimentCLITest|RV32IDifferentialTest|RV32IBinaryClockCLITest)$'
```

Run serially on a memory-constrained host. These include small structural/reference checks but not a full compiled benchmark suite. See each reference for the separate documentation, loader, profile, and timing-unit tests.
This command selects eight focused tests. To run every currently registered
benchmark-label test, including campaign recovery and compatibility ELF smoke
checks, use `ctest --test-dir build -L benchmark --output-on-failure -j1`.
