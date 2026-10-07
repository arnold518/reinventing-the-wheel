# Embench: Cubic equations

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Solve fixed cubic equations with floating-point arithmetic.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `cubic` |
| Pinned source | `src/cubic/basicmath_small.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh cubic` |
| Resulting image | `build/rv32i-embench/artifacts/cubic/cubic.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

The body calls the upstream cubic solver for several coefficient sets. The first two include (1,-10.5,32,-30) and (1,-4.5,17,-30); solver branches select the real-root case.

## Expected result

The verifier checks three roots 2, 6, 2.5 for the first saved result and one root 2.5 for the second, using the upstream floating-point comparison.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Soft floating point and calls to mathematical routines such as sqrt, pow, cos, and acos. The current freestanding port does not automatically supply a complete libm.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/cubic/basicmath_small.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
