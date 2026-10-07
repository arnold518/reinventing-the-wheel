# Streaming copy and checksum

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Produce regular sequential loads and stores while retaining an independently checkable output image.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `stream` |
| Selection | `--workload stream` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

n seeded words at input 0x1000; output at 0x2000.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

At each outer iteration reset both pointers, copy each word, and add it to the checksum.

```cpp
for (size_t i = 0; i < n; ++i) { output[i] = input[i]; sum += input[i]; }
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Every output word equals the corresponding input word, and result=N*sum(input) modulo 2^32.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

With zero-wait FF memory this measures instruction scheduling and port activity, not a cache miss rate. Reuse the same image for a future memory hierarchy, while recording that new target configuration.

```sh
build/rv32i_experiment --core advanced --workload stream \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/cpp/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
