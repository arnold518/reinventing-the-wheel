# Bitwise CRC32 kernel

[Generated workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise integer bit operations and data-dependent branches without a lookup table or special instruction.

## Setup

| Property | Value |
| --- | --- |
| Workload ID | `crc32` |
| Selection | `--workload crc32` |
| Generator | `buildRV32IBenchmark() in RV32IBenchmarks.cpp` |
| End condition | EBREAK after the loop and result store |

Use the low byte of each of n seeded words as the input stream.

Use the [shared address map and parameter bounds](index.md#shared-setup). N includes any training iterations; n means size.

## Implementation

Each iteration starts CRC at 0xffffffff. XOR each byte into the CRC; repeat eight logical right shifts, XORing 0xedb88320 when the old low bit was one. Complement at the end and add to x10.

```cpp
crc = 0xffffffff;
for (uint8_t b : bytes) {
  crc ^= b;
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
}
sum += ~crc;
```

This is a behavioral summary of the generated assembly, not the host program being timed.

## Expected result

Result=N*CRC32(bytes) modulo 2^32. The host-side generator computes the expected checksum; every iteration processes the same bytes.

The runner checks the generator's expected result in addition to retirement-by-retirement oracle validation. Output-array assertions, where described, check more than a checksum.

## Measurement and running

This project-owned generated kernel differs from [Embench CRC32](../compiled/crc32.md) in its input, repetition scheme, and verifier. Their instruction counts and scores are not interchangeable.

```sh
build/rv32i_experiment --core advanced --workload crc32 \
  --iterations 2 --size 16 --seed 1  \
  --fidelity behavioral --period 4000 \
  --allocation-limit-mib 512 --max-cycles 4096
```

The explicit period is for a bounded functional check, not a hardware-speed result. For comparisons, use [the serial campaign driver](../campaigns.md), identical case inputs, and a certified per-target clock.

## Source and tests

- `src/analysis/RV32IBenchmarks.cpp`: emitted instructions and independent expected results.
- `tests/RV32IBenchmarkTest.cpp`: generator results, parameter bounds, and edge cases.
- `tests/RV32IBenchmarkCLITest.py` and `tests/BenchmarkCampaignCLITest.py`: [shared execution/campaign checks](../verification/workloads.md).
