# § 4.2.1. Benchmark Architecture

[Benchmark index](index.md) · [Execution contract](execution.md) · [Campaigns](campaigns.md)

## Facilities

These are host-side software interfaces, not selectable structural/behavioral circuits. Their inputs and outputs are configuration objects, prepared images, observations, and result records.

| Facility | Input | Output / responsibility |
| --- | --- | --- |
| `WorkloadDescriptor` | Stable ID, domain, description, tags, parameters | Serializable identity of the work |
| `TargetDescriptor` | Target ID, domain, hardware/fidelity parameters | Serializable identity of the tested system |
| `Metric` | Name, numeric value, unit | One explicitly named measurement |
| `RunRecord` | Descriptors, status, validation, metrics, fingerprints | Common schema-1 result envelope |
| `RV32IWorkloadRequest` | Fixed ID, generated parameters, or ELF; memory capacity | Request to prepare one RV32I case |
| `prepareRV32IWorkload` | Workload request | Image blocks, expected outputs, completion rule, region markers and image fingerprint |
| `RV32IExperimentConfig` | Core, fidelity, predictor, memory, clock, budgets and case | Complete configuration for one fresh run |
| `runRV32IExperiment` | Configuration | Verified execution and domain-specific measurements |
| `rv32i_experiment` | CLI arguments | One JSON report, with common `benchmark_run` envelope |
| `experiment.py` | Case/target matrix and repetition settings | RV32I campaign, preflight, clocks, raw runs and summaries |
| `tools/benchmarks/run.py` | Domain and campaign options | Common RV32I/memory entry point and portable record stream |

## Data flow

| Phase | Operation | Boundary |
| --- | --- | --- |
| Select | Choose the workload independently of the core | Do not change input data to favor one target |
| Prepare | Encode a fixed/generated program or load ELF segments | Validate parameters, capacity, entry and image identity |
| Construct | Build the selected core and endpoints; record implementation manifest | Fidelity is explicit; no silent substitution |
| Execute | Reset, clock, observe attempts/transactions, enforce budgets | Driver observes the circuit; it does not perform instructions for a structural core |
| Validate | Check oracle state, independent expectations where supplied, and final data memory | Correct completion is separate from host runtime |
| Record | Serialize domain counters and the common envelope | Preserve units, status and provenance |
| Compare | Check comparable inputs/outcomes, then summarize measurements | Do not compare different work as one score |

The common model is deliberately data-only. It does not contain an instruction interpreter, cache scheduler, or generic accelerator simulator. RV32I retirement stays in its runner; memory request/response sequencing stays in memory tests.

## Workload preparation

The fixed path loads the exact registered program and initial data blocks. It requires reset PC and registers to be zero. The generated path calls either the small legacy emitter or `buildRV32IBenchmark`; parameterized kernels attach independently computed output expectations and optional body markers. The ELF path validates the image and copies each loadable segment into both logical memories, preserving their separate identity during execution.

The workload fingerprint includes whether each block belongs to instruction or data memory, its address, and bytes. It identifies the prepared image; a separate ELF/source SHA-256 identifies provenance. Matching a short image hash alone does not establish correctness.

## Result contract

| Field | Meaning |
| --- | --- |
| `schema` | Common envelope version, currently 1 |
| `workload`, `target` | IDs, domains, and parameter maps |
| `status` | `completed`, `failed`, `incomplete`, or `unsupported` |
| `validated` | Whether the run's configured validation established success |
| `simulatedTicks` | Optional simulated duration; RV32I stores cycles × scheduling period, not automatically certified hardware time |
| `hostSeconds` | Host execution cost |
| `metrics` | Named values with units; domain-specific counters remain here |
| `fingerprints` | Image/state/build identities supported by the producer |

The RV32I JSON also retains its richer top-level fields such as `whole_program_cycles`, `observations`, timing reports, and implementation manifest. Do not confuse those snake-case fields with the portable envelope's camel-case fields. Schema validation rejects invalid descriptors, duplicate metrics and inconsistent records; it does not run the workload.

A failing CLI may exit before producing a record. A missing record or missing `COMPLETE` is not a successful zero-duration run.

## Other domains

The memory campaign invokes `MemoryStorageBenchmark`, `MemoryEndpointBenchmark`, `FFMemoryBenchmark`, and `SRAMBenchmark` serially. Each executes its own validated scenarios and emits the same record schema. Those host timings measure simulator cost, not a measured physical SRAM/DRAM access latency. The campaign currently has fewer provenance fields than the RV32I driver; do not assume it already records every RV32I clock/manifest artifact.

A future cache can be measured through request traces and as an RV32I memory target. An accelerator can report completed operations and utilization. Both require a real domain runner and correctness contract; merely assigning a new `domain` string does not implement them.

## Source and tests

Canonical interfaces: `include/benchmark/Benchmark.hpp`, `include/analysis/RV32IWorkloads.hpp`, and `include/analysis/RV32IExperiment.hpp`. Implementations: `src/benchmark/`, `src/analysis/RV32IWorkloads.cpp`, and `src/analysis/RV32IExperiment.cpp`.

[Schema/campaign tests](verification/infrastructure.md) validate serialization, catalogue identity, and both currently implemented campaign frontends.
