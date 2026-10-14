# Middle-Scale Compiled Equivalence

[Compiled workload catalogue](index.md) · [Campaigns](../campaigns.md) · [Fidelity equivalence](../verification/equivalence.md)

## Purpose

Run real compiled algorithms completely in both structural-reference and behavioral cores, between the tiny 64-instruction C fixture and multi-million-instruction full Embench runs. These are **reduced-work equivalence workloads, not full Embench results**. They do not replace the full suite or its timing data.

## Setup

| Property | Contract |
| --- | --- |
| Suite | `embench-middle-v1`, defined in `tools/rv32i-benchmarks/embench-middle.json` |
| Upstream | Embench 1.0, commit `0466a18e4f6b47e19598d7c6ba72916d54b68f65` |
| Compiler | Pinned xPack GCC 12.4.0-2, GCC 12.4.0, `rv32i/ilp32`, `-O2`; newlib/libm/libgcc as needed |
| Work | One complete upstream kernel invocation; original input data and verifier |
| Warmup | No in-program warmup; default campaign has no host warmup |
| Memory/startup | Existing 256 KiB ELF contract, 16 KiB stack, zero-wait FF endpoints |
| Completion | Retired `tohost=1` verdict store after the original verifier succeeds |
| Default targets | Single-cycle, baseline, advanced/two-bit with 16 predictor entries: three targets, not seven |

The three ELF names are distinct from the full suite. For example, the common run record identifies CRC32 as `elf/embench-middle-crc32.elf`, not `elf/crc32.elf`.

| Source | Complete work retained | ELF | Executed instructions¹ |
| --- | --- | --- | ---: |
| `crc32` | CRC over 1,024 deterministic pseudo-random bytes, including generator initialization | `embench-middle-crc32.elf` | 35,907 |
| `nettle-sha256` | SHA-256 initialization, message processing, digest generation and comparison | `embench-middle-nettle-sha256.elf` | 8,767 |
| `sglib-combined` | Array quicksort, linked-list sorting, hash table, queue, heap and red-black tree operations | `embench-middle-sglib-combined.elf` | 105,470 |

¹ Instruction-reference preflight counts for this pinned build, including startup and verification. They are not structural run results or simulated cycle counts.

## Implementation

`tools/rv32i-benchmarks/embench/middle.c` includes the selected, unchanged upstream C translation unit. This gives the wrapper access to its private `benchmark_body` without copying or patching the algorithm. It replaces only the full-suite entry point:

```cpp
int main(void) {
    initialise_board();
    initialise_benchmark();
    start_trigger();
    int result = benchmark_body(1);
    stop_trigger();
    return verify_benchmark(result) != 1;
}
```

The full port instead performs one warmup and calls the upstream `benchmark()`, which applies its local repetition factor. The middle wrapper skips warmup entirely and fixes the repetition count to one; it does not shrink the input, stop at an execution prefix, supply a precomputed output, or accept a missing verifier. The ordinary startup code still publishes the verdict.

The native workload loader and CPU implementation are unchanged. These images enter through the existing ELF path, use the same core profiles and emit the same `benchmark_run` schema as other processor workloads. No second simulator or special middle-suite execution engine is involved.

The middle build saves intermediates with deterministic object names. This removes random temporary names from ELF symbol metadata, permitting byte-identical ELF rebuilds without stripping useful symbols. The full-profile build flags remain unchanged.

## Expected result

| Source | Original verifier retained |
| --- | --- |
| `crc32` | Returned CRC value modulo 32,768 is 11,433 |
| `nettle-sha256` | All 32 digest bytes equal the upstream expected digest |
| `sglib-combined` | Result is 15,050; sorted array, list order, hash-table membership and heap-integrity checks pass |

Each checked structural and behavioral run independently checks architectural state against the instruction oracle and checks the complete final data-memory image. Their aggregate signatures must then agree: cycles, instruction accounting, observation counters, workload fingerprint and final-state fingerprint. Different core designs must agree on architectural results, not cycle counts.

This is complete-program aggregate equivalence, **not a cycle-by-cycle wire-trace comparison or a formal proof**. The structural reference expands the CPU datapath but keeps the existing compact register/storage abstractions. The short differential suite remains responsible for settled cycle-trace checks.

The [current result coverage](../../experiments.md#completed-campaign-coverage)
separates the checked-in ELF fixtures from the projected full Embench suite.
This reduced-work middle suite has not been rerun on the current source, so
§ 4.7 does not present its older measurements as current results.

## Measurement and running

Build explicitly from the existing pinned source checkout and verified multilib compiler; neither ordinary tests nor the campaign download or compile automatically. Output directories must be new.

```sh
python3 tools/rv32i-benchmarks/build-embench-multilib.py --profile middle \
  --compiler build/toolchains/xpack-riscv-none-elf-gcc-12.4.0-2/bin/riscv-none-elf-gcc \
  --output build/rv32i-embench-middle/artifacts

python3 tools/benchmarks/run.py rv32i \
  --preset compiled-middle --output build/experiments/middle-equivalence
```

The preset verifies the suite identity, source revision, repetition/warmup settings, exact three-image inventory, successful build records and ELF hashes before starting. A full-suite directory, extra ELF, missing case, changed binary or failed build is rejected. `--compiled-dir` selects another build of this same suite. Whole-program selection is mandatory; workload and PC-window overrides are rejected.

The default matrix has **three programs × three targets = nine paired cases**. Each case performs one oracle-checked behavioral run, one oracle-checked structural-reference run, and one behavioral measurement that must match the checked signature. `--policies` can explicitly expand the advanced predictor set; five policies make 21 paired cases.

Processes run serially with a 512 MiB allocation cap, no waveform history, certified binary-startup clocks, a 500,000-cycle cap, a 20-billion-event cap and a 7,200-second timeout per subprocess. Event and wall-clock budgets allow the expanded datapath; they do not force a run to consume that amount. Complete structural comparisons can take hours. Use a separate output directory and avoid concurrent timing studies when comparing host execution costs.

For a smaller initial campaign, use the same general interface with just SHA-256:

```sh
python3 tools/benchmarks/run.py rv32i --timeout 1800 \
  --output build/experiments/middle-sha-equivalence -- \
  --elf build/rv32i-embench-middle/artifacts/nettle-sha256/embench-middle-nettle-sha256.elf \
  --cores single,baseline,advanced --policies two-bit --reference-check \
  --clock-mode binary-safe --allocation-limit-mib 512 \
  --max-cycles 500000 --max-events 20000000000 --warmups 0 --repetitions 1
```

The standard output files contain both fidelity records and their manifests. Directory campaigns additionally embed `builds.json` and its hash in provenance. `COMPLETE` means every selected comparison and measurement passed; a built ELF or successful instruction-reference preflight alone does not establish structural equivalence. No result here implies a completed full-Embench structural run.

## Source and tests

- `tools/rv32i-benchmarks/embench-middle.json`: exact suite inventory and scaling contract.
- `tools/rv32i-benchmarks/embench/middle.c`: reduced-work entry point; unchanged upstream sources remain under `build/`.
- `tools/rv32i-benchmarks/build-embench-multilib.py`: serial, pinned full/middle builds and provenance.
- `tools/benchmarks/run.py`: preset validation and delegation to the common campaign driver.
- `tests/checks/RV32IMiddleBenchmarkTest.py`: valid/invalid manifests, checksums, accidental full-suite selection, preset flags and optional complete-ELF instruction preflight.

```sh
python3 tests/checks/RV32IMiddleBenchmarkTest.py --build-dir build \
  --artifacts build/rv32i-embench-middle/artifacts
```

Without `--artifacts`, `RV32IMiddleBenchmarkTest` runs the bounded configuration tests and explicitly skips the real-ELF preflight. Full structural comparisons are an explicit campaign, not a hidden addition to every CTest run.
