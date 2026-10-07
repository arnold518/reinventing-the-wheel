# Dependency distance

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Vary how many independent accumulators are interleaved while preserving the arithmetic instruction count.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `dependency` |
| Selection | `--workload dependency` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

x16..x23 start at 1..8. `--distance` is one of 1, 2, 4, 8; default 1.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Execute 64 ADDI +1 instructions per outer iteration, rotating across the first distance accumulators. Sum all eight at completion.

```cpp
for (unsigned k = 0; k < 64; ++k) ++chains[k % distance];
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result = (36 + 64*N) modulo 2^32, independent of distance. For N=2 the result is 164. The identical checksum permits a controlled distance comparison.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Vary only distance and target. A larger distance gives the producer more cycles before its next consumer, while body work stays fixed.

```sh
build/rv32i_experiment --core advanced --workload dependency \
  --iterations 2 --size 16 --seed 1 --distance 4 \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
