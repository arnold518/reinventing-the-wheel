# Embench: CRC32

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Compute CRCs over a deterministic pseudo-random input stream.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `crc32` |
| Pinned source | `src/crc32/crc_32.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh crc32` |
| Resulting image | `build/rv32i-embench/artifacts/crc32/crc32.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. A CRC32 artifact exists in the inspected checkout; historical execution evidence is not a current-core rerun. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Each body repetition reseeds the upstream pseudo-random generator with zero and runs the original CRC routine. The upstream local repetition factor is 170 at CPU_MHZ=1.

## Expected result

`verify_benchmark` requires the returned value to equal 11433.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Bit manipulation, loop control, and software arithmetic. This is not the shorter project-generated `--workload crc32` kernel.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/crc32/crc_32.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
