# Nested direct and indirect calls

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise link registers, stack save/restore, and call/return control flow.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `calls` |
| Selection | `--workload calls` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

x2/sp starts at 0x4000. x12 holds the outer function address. Result x10 starts at zero.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Each outer iteration calls the function once with JAL and once with JALR. The function allocates 16 bytes, saves ra, calls a leaf, restores ra and sp, and returns. The leaf adds three.

```cpp
call_function();   // direct call; leaf adds 3
call_function();   // indirect call; leaf adds 3
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result=6*N; sp returns to 0x4000. For N=2, result=12. The image and oracle also check actual call/return flow and memory effects.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

This exercises the current jump machinery; it does not claim that the core has a return-address stack or indirect-target predictor.

```sh
build/rv32i_experiment --core advanced --workload calls \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/cpp/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
