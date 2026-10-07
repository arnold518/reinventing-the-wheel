# Alternating branch loop

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise a repeating taken/not-taken conditional outcome with a small instruction footprint.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `alternating` |
| Selection | `--workload alternating` |
| Generator | `legacyProgram() in RV32IWorkloads.cpp` |
| End condition | EBREAK after the loop |

`x1 = iterations`; x2 and x3 start at zero. No data memory input.

N means the requested iteration count. Registers start from reset; the loop's x2 is not an ABI stack pointer.

## Implementation

Toggle the low bit of x2. BNE skips an increment of x3 when x2 is nonzero; a separate loop branch decrements x1.

```cpp
x2 ^= 1;
if (x2 == 0) ++x3;
--x1;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

x2=N%2, x3=floor(N/2), x1=0. For N=12: x2=0, x3=6. The first inner branch is taken.

The runner validates the executed architectural state with the instruction oracle. This legacy generator does not publish a separate `expected_result` checksum; the equations above describe its loop.

## Measurement and running

There are two static branch sites, with different outcome sequences. Compare policies on identical iterations; do not treat every branch as alternating.

```sh
build/rv32i_experiment --core advanced --workload alternating \
  --iterations 2  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IWorkloads.cpp`: emitted instructions.
- `tests/checks/RV32IExperimentCLITest.py`: legacy loops across all three cores.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
