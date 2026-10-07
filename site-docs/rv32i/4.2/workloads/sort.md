# Unsigned insertion sort

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise data-dependent branches, indexed loads/stores, and a nontrivial output verifier.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `sort` |
| Selection | `--workload sort` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

n seeded 32-bit values at 0x1000; working array at 0x2000.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

For each outer iteration, copy the original input to the working array, insertion-sort it in unsigned order, and accumulate the sorted values.

```cpp
output = input;
insertion_sort_unsigned(output);
for (auto v : output) sum += v;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

The entire output array must equal the independently sorted input. Result=N*sum(input) modulo 2^32. The checksum alone cannot prove that the array is sorted.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Use small sizes first: insertion sort can require quadratic work. The core-gap and generated-regression presets cap this family's iterations at two, not its size.

```sh
build/rv32i_experiment --core advanced --workload sort \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/cpp/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
