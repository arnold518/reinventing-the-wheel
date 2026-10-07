# § 4.2.4. Generated Workloads

[Benchmark reference](../index.md) · [Fixed programs](../programs/index.md) · [Campaigns](../campaigns.md)

## Catalogue

The native catalogue contains these 15 generated families in addition to 22 fixed programs. A family is not one timing result: parameters, target, policy, and measurement window define the actual case.

| Workload ID / CLI name | Purpose | Generator |
| --- | --- | --- |
| [`dependent`](dependent.md) | Dependent integer recurrence | Legacy loop |
| [`alternating`](alternating.md) | Alternating branch loop | Legacy loop |
| [`nested`](nested.md) | Nested counted loops | Legacy loop |
| [`independent`](independent.md) | Independent ALU chains | Parameterized kernel |
| [`dependency`](dependency.md) | Dependency distance | Parameterized kernel |
| [`load-use`](load-use.md) | Immediate load-use pairs | Parameterized kernel |
| [`load-scheduled`](load-scheduled.md) | Scheduled load-use pairs | Parameterized kernel |
| [`branch-pattern`](branch-pattern.md) | Controlled branch outcomes | Parameterized kernel |
| [`branch-alias`](branch-alias.md) | Predictor table aliasing | Parameterized kernel |
| [`calls`](calls.md) | Nested direct and indirect calls | Parameterized kernel |
| [`stream`](stream.md) | Streaming copy and checksum | Parameterized kernel |
| [`pointer-chase`](pointer-chase.md) | Dependent pointer chasing | Parameterized kernel |
| [`sort`](sort.md) | Unsigned insertion sort | Parameterized kernel |
| [`crc32`](crc32.md) | Bitwise CRC32 kernel | Parameterized kernel |
| [`scan`](scan.md) | Byte scan and classification | Parameterized kernel |

## Shared setup

The three legacy loops accept an iteration count and use their own registers; they do not implement the parameterized body's automatic training/window markers. The other 12 kernels share this setup:

| Property | Contract |
| --- | --- |
| Iterations | 1..1,000,000 measured iterations; optional training plus measured total also at most 1,000,000 |
| Size | 1..256 data elements, default 16; does not resize fixed-shape ALU/call/branch bodies |
| Seed | Nonzero 32-bit integer, default 1; xorshift32 makes generated input reproducible |
| Memory | Separate zero-filled instruction/data images; default 256 KiB each |
| Data addresses | Input/nodes 0x1000, output 0x2000, result word 0x3000 |
| Stack | Calls kernel initializes sp=0x4000; other kernels are not ABI applications |
| Registers | Reset to zero; x28 is the outer counter and x10 is the result accumulator |
| Termination | Store the checksum at 0x3000, then EBREAK |
| Arithmetic | Unsigned 32-bit wraparound unless stated otherwise |

`--distance` may be nondefault only for dependency; `--branch-spacing` only for branch-alias; `--pattern` only for branch-pattern. Shape parameters recorded in a case are not necessarily used by every family.

## Behavioral description and checking

Each page describes the generated instruction sequence using compact pseudocode, not a replacement simulator implementation. The actual generator emits RV32I machine words. The runner checks those words on the selected core against the instruction oracle; parameterized kernels additionally provide host-computed expected checksums and, where applicable, expected output bytes.

In the page formulas, **N = measured iterations + training iterations**, and n is the data size. Whole-program final state includes training. A body-window counter covers only the selected measured region; its result need not be a checksum of that region alone.

Use `--measure-body --training-iterations 32` for a parameterized body measurement after 32 real in-program training iterations. Without a body window, setup, loop control, final checksum reduction/store, and termination all belong to the whole-program measurement. See [execution](../execution.md) for exact retirement/window accounting.

## Running

```sh
build/rv32i_experiment --core advanced --workload scan \
  --iterations 2 --size 16 --seed 1 --fidelity behavioral \
  --period 4000 --allocation-limit-mib 512 --max-cycles 4096
```

Here 4000 is an explicit conservative **scheduling** period for a small functional run, not a measured best clock or performance claim. Use a [clock-certified campaign](../campaigns.md) for cross-core timing comparisons. These headless generated workloads do not currently have individual embedded visualizer scenarios.

## Source and tests

`src/analysis/RV32IBenchmarks.cpp` implements the 12 kernels and independent expected results. `src/analysis/RV32IWorkloads.cpp` owns the three legacy loops and shared preparation. [Workload validation](../verification/workloads.md) documents host-result, CLI, campaign, and corner-case tests.
