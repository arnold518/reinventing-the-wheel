# Embench: AES encryption and decryption

[Compiled workload catalogue](index.md) · [Execution contract](../execution.md)

## Purpose

Encrypt and decrypt a fixed 256-byte plaintext using a 256-bit key.

## Setup

| Property | Value |
| --- | --- |
| Source workload | `nettle-aes` |
| Pinned source | `src/nettle-aes/nettle-aes.c` |
| Revision | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Build selection | `bash tools/rv32i-benchmarks/build-embench.sh nettle-aes` |
| Resulting image | `build/rv32i-embench/artifacts/nettle-aes/nettle-aes.elf` |
| Completion | Upstream verifier, main return, then committed tohost verdict |

These are the upstream fixed inputs and repetition rules, not the native generator's size/iterations options. This page documents source behavior; a successful build or full current-core run is not claimed. See [port limitations](index.md#embench-source-catalogue).

## Implementation

Set an AES encryption key, encrypt the buffer, set a decryption key, and decrypt it, preserving the upstream body repetitions.

## Expected result

Every encrypted byte must equal the reference ciphertext and every decrypted byte must equal the original plaintext.

The shared upstream main converts verifier failure into a nonzero return. The CircuitSim startup bridge publishes tohost=1 on success or 3 on failure. A checked instruction prefix cannot establish this final result.

## Measurement and running

Key expansion, lookup tables, XORs, and buffer traffic. No AES hardware instructions are enabled in the RV32I build.

Build this source explicitly, inspect its ELF size/instruction requirements, then follow the [bounded prefix and campaign procedure](index.md#build-and-execution). Full-run timing includes startup, warmup, benchmark body, verifier, and verdict unless a separate valid measurement window is selected.

## Source and tests

The [pinned upstream source](https://github.com/embench/embench-iot/blob/0466a18e4f6b47e19598d7c6ba72916d54b68f65/src/nettle-aes/nettle-aes.c) contains the benchmark body and verifier. Other files in that source directory may provide helper implementations. Local port files are under `tools/rv32i-benchmarks/embench/`.

The ordinary repository regression suite tests the ELF loader/runner using small fixtures; it does **not** download and run this workload automatically. Keep its external verifier result, runner/oracle result, and any structural/behavioral comparison as separate recorded checks.
