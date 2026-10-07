# Embench: N-body energy calculation

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Compute the energy of five fixed solar-system bodies in this pinned variant.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `nbody` |
| Pinned source | `src/nbody/nbody.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh nbody` |
| Resulting image | `build/rv32i-embench/artifacts/nbody/nbody.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Offset the total momentum and accumulate the energy calculation 100 times per body repetition. This source variant does not advance positions through a time-stepping integration loop.

## Expected result

The accumulated energy is compared with -16.907516382852478 using the upstream double comparison; the verifier also checks body state against reference data.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Pairwise arithmetic and square roots, implemented using software floating point/library routines. Do not describe this specific image as a long evolving gravitational simulation.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/nbody/nbody.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
