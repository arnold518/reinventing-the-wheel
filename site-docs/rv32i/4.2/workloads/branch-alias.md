# Predictor table aliasing

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Place opposite-outcome branches at a controllable PC spacing.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `branch-alias` |
| Selection | `--workload branch-alias` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

`--branch-spacing` is a power of two from 4 through 256, default 16, measured in instruction words.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

The first BEQ is always taken and skips an addition of 99. Padding instructions increment x9. The second BNE is always not taken and allows x10+=1. Finally include x9 in the checksum.

```cpp
// S is the static distance in instruction words.
padding_count += S - 2;
++sum;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result=N*(S-1) modulo 2^32. With S=16 and N=2, result=30. The two branch PCs are 4*S bytes apart.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Spacing may map the two branches to the same predictor entry, depending on policy/indexing and `--entries`. The outer-loop branch can also interfere; this is a concrete collision workload, not an isolated abstract table experiment.

```sh
build/rv32i_experiment --core advanced --workload branch-alias \
  --iterations 2 --size 16 --seed 1 --branch-spacing 16 \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
