# Nested counted loops

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise two nested branch sites with repeated inner-loop exits.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `nested` |
| Selection | `--workload nested` |
| Generator | `legacyProgram() in RV32IWorkloads.cpp` |
| End condition | EBREAK after the loop |

`x1 = iterations`; x3 starts at zero. Each outer iteration initializes x2=8.

N means the requested iteration count. Registers start from reset; the loop's x2 is not an ABI stack pointer.

## Implementation

Run eight increments of x3, using x2 as the inner countdown; then decrement the outer x1 counter.

```cpp
for (unsigned j = 0; j < 8; ++j) ++x3;
--x1;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

x3=8*N, x1=x2=0. For N=12, x3=96. Each inner BNE is taken seven times and then not taken.

The runner validates the executed architectural state with the instruction oracle. This legacy generator does not publish a separate `expected_result` checksum; the equations above describe its loop.

## Measurement and running

Useful for predictor loop behavior; also includes arithmetic dependencies and outer-loop control. No call stack is involved.

```sh
build/rv32i_experiment --core advanced --workload nested \
  --iterations 2  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IWorkloads.cpp`: emitted instructions.
- `tests/checks/RV32IExperimentCLITest.py`: legacy loops across all three cores.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
