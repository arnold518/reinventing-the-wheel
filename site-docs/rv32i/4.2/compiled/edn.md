# Embench: DSP kernel collection

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Run several fixed-point signal-processing kernels in one workload.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `edn` |
| Pinned source | `src/edn/libedn.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh edn` |
| Resulting image | `build/rv32i-embench/artifacts/edn/edn.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

The body initializes sample/coefficient arrays, then runs vector multiply, multiply-accumulate, FIR variants, lattice synthesis, IIR, codebook search, and a JPEG DCT kernel.

## Expected result

Compare all 200 output words with a reference vector, plus scalar checks c=10243 and d=e=-441886230.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Integer arithmetic and regular array accesses with different loop shapes. The final verifier covers specific observables, not every possible input to every DSP kernel.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/edn/libedn.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
