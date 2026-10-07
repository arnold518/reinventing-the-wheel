# Independent ALU chains

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Expose work that does not depend on the immediately preceding ALU result.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `independent` |
| Selection | `--workload independent` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

Eight accumulators x16..x23 start at 1..8. The input-data size does not set this fixed 64-operation body.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Issue 64 instructions in rounds across eight chains: ADDI +3, XORI 85, SLLI 1, SRLI 1, repeated twice per chain. The final reduction sums all eight accumulators.

```cpp
for (unsigned round = 0; round < 2; ++round)
  for (auto& v : chains) v = uint32_t((v + 3) ^ 85) << 1 >> 1;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Starting from chains={1,2,3,4,5,6,7,8}, apply the body N times and sum modulo 2^32. Shift-left truncates before logical shift-right. The generator computes this expected checksum independently of the instruction oracle.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Compare with the dependency-distance sweep. These are independent chains, not instruction-level parallel execution: all current cores remain single-issue.

```sh
build/rv32i_experiment --core advanced --workload independent \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
