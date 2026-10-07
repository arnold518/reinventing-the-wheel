# Immediate load-use pairs

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Measure the dependency from a loaded value to its immediately following arithmetic consumer.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `load-use` |
| Selection | `--workload load-use` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

n seeded 32-bit words at 0x1000; checksum x10 and independent counter x7 start at zero.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

For every word, issue LW into x5, add x5 to x10 immediately, then increment x7. Repeat the same unrolled sequence N times.

```cpp
for (auto v : input) { sum += v; ++independent_counter; }
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result = N*sum(input) modulo 2^32; x7=N*n. Input is unchanged. The same result is required from `load-scheduled`.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Compare directly with [load-scheduled](load-scheduled.md). The instruction multiset and data are identical; only the position of an independent instruction changes.

```sh
build/rv32i_experiment --core advanced --workload load-use \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
