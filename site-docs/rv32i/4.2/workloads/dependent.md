# Dependent integer recurrence

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

A compact legacy forwarding/dependency loop; unlike `dependency`, it has no configurable interleaving distance.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `dependent` |
| Selection | `--workload dependent` |
| Generator | `legacyProgram() in RV32IWorkloads.cpp` |
| End condition | EBREAK after the loop |

`x1 = iterations`, `x2 = 0`, `x3 = 0`. No input data or ABI stack.

N means the requested iteration count. Registers start from reset; the loop's x2 is not an ABI stack pointer.

## Implementation

Each outer iteration increments x2, doubles that value into x3, then adds x3 back into x2. Decrement x1 and branch until zero.

```cpp
x2 = 3 * (x2 + 1);  // uint32_t wraparound
x1 -= 1;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

After N iterations, x2 follows v[0]=0, v[k+1]=3*(v[k]+1) modulo 2^32. x3 is twice the last incremented value; x1=0. For N=2: x2=12, x3=8.

The runner validates the executed architectural state with the instruction oracle. This legacy generator does not publish a separate `expected_result` checksum; the equations above describe its loop.

## Measurement and running

Observe dependence stalls/forwarding and loop overhead. This is the older recurrence fixture, not the 64-operation distance sweep.

```sh
build/rv32i_experiment --core advanced --workload dependent \
  --iterations 2  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IWorkloads.cpp`: emitted instructions.
- `tests/checks/RV32IExperimentCLITest.py`: legacy loops across all three cores.
- `tests/checks/RV32IBenchmarkCLITest.py` and `tests/checks/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
