# Dependent pointer chasing

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Force a subsequent memory address to depend on a prior load.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `pointer-chase` |
| Selection | `--workload pointer-chase` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

n nodes at 0x1000; each is {next address, 32-bit payload}. A seeded shuffle links all nodes into one ring. Traversal starts at node zero.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Load the next-node address into the address register, then load that node's payload. Make n steps per outer iteration.

```cpp
for (size_t i = 0; i < n; ++i) { node = node->next; sum += node->payload; }
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Each complete ring traversal visits every payload once. Result=N*sum(payloads) modulo 2^32. The one-node case points back to itself.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

Compare with stream while acknowledging different instruction sequences. On current FF memory there is no modeled DRAM/cache locality penalty; the dependent-address chain is still real.

```sh
build/rv32i_experiment --core advanced --workload pointer-chase \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
