# Scheduled load-use pairs

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Test whether one independent instruction between load and use hides a pipeline load-use bubble.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `load-scheduled` |
| Selection | `--workload load-scheduled` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

The same n seeded words and initial registers as `load-use`.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

For every word, issue LW, increment independent x7, then add the loaded x5 to x10. No extra operation is inserted.

```cpp
for (auto v : input) { ++independent_counter; sum += v; }
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result = N*sum(input) modulo 2^32; x7=N*n, exactly as in `load-use`.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

The advanced pipeline should need fewer load-use bubbles when this spacing suffices. Compare observed LOAD_USE counts as well as cycles; memory wait states can change the benefit.

```sh
build/rv32i_experiment --core advanced --workload load-scheduled \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/cpp/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
