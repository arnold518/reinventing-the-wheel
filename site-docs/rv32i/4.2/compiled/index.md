# § 4.2.5. Compiled and External Workloads

[Benchmark reference](../index.md) · [Execution](../execution.md) · [Campaigns](../campaigns.md)

## Image contract

| Property | Required behavior |
| --- | --- |
| Format | Statically linked ELF32, little-endian, RISC-V machine ID |
| ISA / ABI | RV32I / ilp32; no compressed, multiply/divide, floating-point, atomic, or privileged instructions |
| Entry | Four-byte aligned; the experiment runner currently requires reset entry zero |
| Segments | Non-overlapping PT_LOAD ranges with equal virtual/physical addresses and valid file/memory sizes |
| Memory | All loaded ranges fit the 256 KiB loader limit and the configured device capacity |
| Initial contents | Segment bytes loaded into both separate I and D memories; BSS tails zero-filled |
| Runtime | Bare metal; no operating system, dynamic loader, or Linux syscalls |
| Success | A committed store publishes `tohost = 1`; failure verdict 3 is rejected |
| Default tohost | 0x3ffc0, word aligned; native runner supports an explicit address |
| Stack | Startup code establishes it; the simulator does not infer an ABI stack |

The I and D images begin with the same ELF segments, but are not coherent aliases: subsequent data stores do not modify instruction memory. A successful load does not statically prove the whole image uses only RV32I; unsupported executed instructions still trap.

The loader validates segment ranges, overlap, alignment, and executable entry coverage. The runner stops on the **retired verdict write**, not just because a speculative/in-flight store has touched memory. It rejects a halt/trap before the verdict. A bounded prefix has not run the final verifier and is not a completed benchmark.

## Checked-in fixtures

| Page | Origin | What it checks |
| --- | --- | --- |
| [Self-checking smoke](smoke.md) | External assembly | ELF loading, arithmetic, memory, calls, and tohost |
| [Compiled equivalence](compiled-equivalence.md) | Project-owned C compiled with pinned GCC | Compiler-generated stack/calls/branches and complete structural/behavioral comparison |

Neither fixture is an Embench workload. Their ELF images and checksums are checked in, so ordinary tests need no download, Docker, or cross-compiler.

## Middle-scale compiled equivalence

[Middle-scale equivalence](middle-equivalence.md) uses real Embench CRC32, SHA-256 and SGLIB algorithms with their unchanged input data and verifiers, but only one complete kernel invocation and no warmup. Distinct `embench-middle-*.elf` names keep these reduced-work images separate from the full suite. The `compiled-middle` campaign preset runs both fidelities on all three cores, using the existing ELF loader and result schema.

## Embench source catalogue

The source lock pins Embench 1.0 at `0466a18e4f6b47e19598d7c6ba72916d54b68f65`. The historical CRC32 port uses GCC 16.1.0 in an immutable compiler image. The full/middle multilib port uses separately pinned xPack GCC 12.4.0-2 with RV32I libraries; these are different ELF cohorts. Each page below describes the **pinned source**, its verifier, and its relevance.

| Source workload | Main behavior |
| --- | --- |
| [`aha-mont64`](aha-mont64.md) | 64-bit Montgomery arithmetic |
| [`crc32`](crc32.md) | CRC32 |
| [`cubic`](cubic.md) | Cubic equations |
| [`edn`](edn.md) | DSP kernel collection |
| [`huffbench`](huffbench.md) | Huffman compression |
| [`matmult-int`](matmult-int.md) | Integer matrix multiplication |
| [`minver`](minver.md) | Small matrix inversion |
| [`nbody`](nbody.md) | N-body energy calculation |
| [`nettle-aes`](nettle-aes.md) | AES encryption and decryption |
| [`nettle-sha256`](nettle-sha256.md) | SHA-256 |
| [`nsichneu`](nsichneu.md) | Generated Petri-net control |
| [`picojpeg`](picojpeg.md) | JPEG decoding |
| [`qrduino`](qrduino.md) | QR encoding |
| [`sglib-combined`](sglib-combined.md) | Generic data structures |
| [`slre`](slre.md) | Regular-expression matching |
| [`st`](st.md) | Statistics |
| [`statemate`](statemate.md) | Generated state machine |
| [`ud`](ud.md) | Integer linear-system solver |
| [`wikisort`](wikisort.md) | Stable comparison sorting |

!!! warning "Source catalogue is not a pass list"
    The pinned source tree contains all 19 workloads. The multilib port can build the full collection with RV32I libc/libm/libgcc; the historical CRC-only toolchain does not provide these RV32I runtime libraries. A named source, a built ELF, an instruction-reference preflight, a completed behavioral run and a structural comparison are different evidence levels. Check each campaign's recorded fidelity and completion artifacts. Middle-suite success is not full-suite success.

## Build and execution

```sh
bash tools/rv32i-benchmarks/build-embench.sh crc32
```

The script fetches only the pinned revision, compiles the selected source directory with `-march=rv32i -mabi=ilp32 -O2`, and records ELF, readelf, size, and SHA-256 artifacts under `build/rv32i-embench/artifacts/NAME/`. The `all` selector attempts every source directory serially; it does not guarantee every port dependency is satisfied.

Start with a bounded prefix, not an unbounded whole-suite run:

```sh
build/rv32i_experiment --core advanced \
  --elf build/rv32i-embench/artifacts/crc32/crc32.elf \
  --fidelity behavioral --period 4000 --max-cycles 4096 \
  --max-events 10000000 --allocation-limit-mib 512 --allow-incomplete
```

The prefix permits calibration and oracle checking but cannot claim the CRC self-check passed. Once budgets are justified, select the built ELF with the [campaign driver](../campaigns.md), raise the cycle/event/time limits explicitly, and require a complete successful verdict.

## What is measured

The **full** port preserves upstream benchmark bodies, local repetition factors, warmup, and verifiers. It supplies board hooks, startup, stack, linker layout, and the verdict bridge. CPU_MHZ is set to 1 and WARMUP_HEAT to 1; this is a workload scaling convention, not a calibrated simulated clock frequency. The separately named [middle suite](middle-equivalence.md) deliberately removes the timing repetitions and warmup, while preserving the actual computation and verifier.

A default full run includes startup, in-program warmup, benchmark repetitions, verification, and the verdict store. It does not automatically isolate the upstream start/stop trigger interval. Explicit PC windows require care around repeated calls and retirement boundaries.

Multiplication, 64-bit arithmetic, and floating point in these C sources execute as RV32I sequences/compiler helpers or need library support. Their presence in C does not add M/F/D extensions to the simulated core. These experiments are not official normalized Embench scores.

## Source and tests

- `src/rv32i/RV32IElfImage.cpp`: loader restrictions.
- `src/analysis/RV32IWorkloads.cpp`, `RV32IExperiment.cpp`: preparation and committed-verdict completion.
- `tools/rv32i-benchmarks/embench.lock`, `build-embench.sh`, `embench/`: provenance and port.
- [Verification reference](../verification/workloads.md): image rejection, smoke execution, and shared-runner tests.
- `docs/rv32i-external-benchmark-report.md`: historical CRC32 evidence on an older core revision, not a fresh result.
