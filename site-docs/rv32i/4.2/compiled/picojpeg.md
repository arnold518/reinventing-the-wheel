# Embench: JPEG decoding

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Decode a fixed embedded JPEG image with the picojpeg decoder.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `picojpeg` |
| Pinned source | `src/picojpeg/picojpeg_test.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh picojpeg` |
| Resulting image | `build/rv32i-embench/artifacts/picojpeg/picojpeg.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Reset the input callback offset, initialize the decoder, and repeatedly decode minimum-coded units until it reports no more blocks.

## Expected result

Compare the retained red, green, and blue block buffers against three 64-byte reference arrays.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Bitstream processing, tables, integer transforms, and branches. The verifier's stored block buffers are the actual checked observable, not a claim about every arbitrary JPEG.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/picojpeg/picojpeg_test.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
