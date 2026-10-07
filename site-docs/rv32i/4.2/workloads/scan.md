# Byte scan and classification

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Provide a compact byte-load/comparison workload with a simple independent expected result.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `scan` |
| Selection | `--workload scan` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

The low byte of each of n seeded words forms a byte array at 0x1000.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Load each byte unsigned and use SLTIU with 128 to count values below 128; repeat the array N times.

```cpp
for (uint8_t b : bytes) sum += (b < 128);
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result=N*count(input bytes <128). The byte classification is unsigned, so bytes 0x80..0xff contribute zero.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Useful as a small generated smoke case, and later as a sequential byte-access case. It does not require a branch for every classification.

```sh
build/rv32i_experiment --core advanced --workload scan \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
