# § 4.2. Benchmarking and Verification

[RV32I reference](../4.1/index.md) · [Building blocks](../4.3/index.md) · [Experiment results](../experiments.md)

Reference for the workload catalogue, execution contracts, verification equipment, and reproducible benchmark campaigns. These facilities observe the cores; they are not additional processor stages.

## Contents

| Section | Reference |
| --- | --- |
| [4.2.1. Benchmark architecture](architecture.md) | Shared data model, workload preparation, domain runners, and campaign responsibilities |
| [4.2.2. Execution and measurement](execution.md) | State, completion, fidelity, clocks, counters, windows, and budgets |
| [4.2.3. Fixed programs](programs/index.md) | All 22 instruction-behavior, fault, and short performance fixtures |
| [4.2.4. Generated workloads](workloads/index.md) | Three legacy loops and twelve parameterized workload families |
| [4.2.5. Compiled workloads](compiled/index.md) | Bare-metal ELF boundary, small fixtures, and nineteen pinned Embench sources |
| [4.2.6. Verification and equivalence](verification/index.md) | Every benchmark validation suite and the limits of its evidence |
| [4.2.7. Campaigns and artifacts](campaigns.md) | Presets, reproducibility, commands, output schema, and interpreting results |

## Terms and inventory

| Term | Meaning | Example |
| --- | --- | --- |
| Workload | Program/input specification and completion contract | `program-01`, `crc32`, an ELF image |
| Case | A workload with particular parameters | `dependency`, 8 iterations, distance 2 |
| Target | Core, predictor, memory and implementation selection | Advanced, two-bit, 16 entries, FF |
| Run | One fresh target executing one case | One verification or timed process |
| Campaign | Selected cases × targets, with verification and repetitions | `core-gap` panel |
| Test suite | Assertions about a generator, runner, contract or implementation | `RV32IDifferentialTest` |

The native catalogue contains **37 entries: 22 fixed programs and 15 generated families**. Parameter combinations are not new catalogue entries. External ELFs are discovered separately; nineteen Embench source directories do not imply nineteen completed runs. The differential suite's 46 pairs are validation cases, not 46 additional registered visualizer programs.

## Three different questions

| Question | Evidence |
| --- | --- |
| Does the program have the intended result? | Explicit fixture outcomes, independently calculated kernel outputs, external self-checkers |
| Do two implementations of the same core agree? | Architectural checks, aggregate reference comparisons, and bounded cycle-level traces |
| Which target executes the workload more efficiently? | Comparable completed runs, declared clocks, counters, and reproducible artifacts |

A behavioral single-cycle model calls the ISA oracle internally; comparing those two alone is not independent validation. A faster host simulator is not automatically a faster modeled processor. Finite passing tests are evidence, not a proof over every possible program.

## Scope

The shared record and campaign entry point already serve RV32I and memory benchmarks. Memory scenarios are summarized in [architecture](architecture.md#other-domains); their device contracts remain in [chapter 3](../../memory/3.1/index.md). Future cache and accelerator runners can reuse these records, but no cache or accelerator performance result is implied here.

Fixed programs have embedded reference simulations and complete checkpoint tables. Generated and compiled workloads currently use the headless runner. Historical measurements belong in [4.7](../experiments.md), not in the workload definitions.
