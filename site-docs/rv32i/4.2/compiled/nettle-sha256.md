# Embench: SHA-256

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Hash a fixed input message with a software SHA-256 implementation.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `nettle-sha256` |
| Pinned source | `src/nettle-sha256/nettle-sha256.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh nettle-sha256` |
| Resulting image | `build/rv32i-embench/artifacts/nettle-sha256/nettle-sha256.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Initialize a context, feed the fixed message through update, and emit the digest for each repetition.

## Expected result

Compare all 32 digest bytes against the stored expected hash.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Integer shifts/rotations, additions, Boolean operations, and round loops. It is a compiled software hash, not a dedicated hash unit.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/nettle-sha256/nettle-sha256.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
