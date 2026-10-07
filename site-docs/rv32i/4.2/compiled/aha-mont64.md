# Embench: 64-bit Montgomery arithmetic

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Compare ordinary and Montgomery-domain modular arithmetic using fixed 64-bit operands and an odd modulus.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `aha-mont64` |
| Pinned source | `src/aha-mont64/mont64.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh aha-mont64` |
| Resulting image | `build/rv32i-embench/artifacts/aha-mont64/aha-mont64.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

The body computes the same repeated modular product through two paths. Montgomery operations avoid dividing by the modulus at every multiplication; explicit high/low arithmetic represents wider intermediate values.

## Expected result

The body reports zero errors; `verify_benchmark` requires that zero result.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

64-bit arithmetic on an RV32I machine expands into multiple instructions and compiler helpers. It is not a test of an implemented 64-bit multiplier.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/aha-mont64/mont64.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
