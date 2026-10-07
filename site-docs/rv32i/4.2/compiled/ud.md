# Embench: Integer linear-system solver

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise a small integer LU-style decomposition/solve routine.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `ud` |
| Pinned source | `src/ud/libud.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh ud` |
| Resulting image | `build/rv32i-embench/artifacts/ud/ud.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Construct the fixed matrix from row/column indices with a modified diagonal, form a right-hand side from row sums, and call ludcmp.

## Expected result

The return value must be zero and the 20-element solution storage must match the expected sequence beginning 0,0,1,1,1,2, with the remaining entries zero.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Division, dependent arithmetic, and array accesses. This source uses integer arithmetic/truncation, not a floating-point linear-algebra library.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/ud/libud.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
