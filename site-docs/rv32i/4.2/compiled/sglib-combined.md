# Embench: Generic data structures

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Exercise several container and sorting operations on a fixed integer dataset.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `sglib-combined` |
| Pinned source | `src/sglib-combined/combined.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh sglib-combined` |
| Resulting image | `build/rv32i-embench/artifacts/sglib-combined/sglib-combined.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

The body works with quicksort, linked lists, a hash table, and a red-black tree, using the benchmark's allocator and fixed input permutation.

## Expected result

The verifier checks list order, successful hash-table lookup of all 100 inserted keys, the sorted 100-element array, allocator bounds, and the accumulated return value 15050.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Pointer manipulation, comparisons, allocation, and traversal. Read the individual verifier checks; passing does not cover all possible container operations.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/sglib-combined/combined.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
