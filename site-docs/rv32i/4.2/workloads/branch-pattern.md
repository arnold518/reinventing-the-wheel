# Controlled branch outcomes

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Provide known taken/not-taken streams for predictor-policy comparisons.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `branch-pattern` |
| Selection | `--workload branch-pattern` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

A 32-word outcome ring. `--pattern` selects taken, not-taken, alternating, bursts, or irregular; the seed makes irregular data deterministic.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Load one outcome, branch if it is one, and increment x10 only on zero. Advance the ring index modulo 32. A separate countdown branch controls the outer loop.

```cpp
if (outcomes[k % 32] == 0) ++sum;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result equals the number of zeros among the first N ring entries. Taken gives 0; not-taken gives N; alternating begins with zero. Bursts uses eight zeros, eight ones, repeated: N=12 gives 8.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

The irregular ring is deterministic, not a nondeterministic predictor test. Its RNG state follows common input generation, so keep both size and seed fixed. Body-window/training options separate cold entry from an already exercised predictor.

```sh
build/rv32i_experiment --core advanced --workload branch-pattern \
  --iterations 2 --size 16 --seed 1 --pattern bursts \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/cpp/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
