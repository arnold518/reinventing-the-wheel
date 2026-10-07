# External RV32I benchmarks

The compatibility ELF executable now delegates execution to the same RV32I
workload preparation and experiment engine used by
[`tools/benchmarks/run.py`](../benchmarks/README.md). New campaigns should use
that common entry point.

For new cross-core/predictor experiments, use
[`rv32i_experiment`](../rv32i-performance/README.md).
It provides a common harness for single-cycle, baseline and advanced cores,
bounded headless execution, explicit measurement windows and JSON results.
The legacy runner below remains available for compatibility; historical
reports are not measurements of the current core revisions.

This directory contains the small CircuitSim port layer and reproducible build
tools for established upstream benchmarks. Upstream source and generated ELF
files stay under `build/`; they are not copied into the repository.

## Embench 1.0

Build the pinned, unmodified Embench CRC32 workload as a static bare-metal
RV32I ELF:

```bash
tools/rv32i-benchmarks/build-embench.sh crc32
```

The pinned source catalogue contains 19 workloads. The `all` selector attempts
each build serially; it is not a promise that every source already links with
the freestanding port. The current link supplies libgcc, not full libc/libm;
workloads needing routines such as memcpy or sqrt may need further port support.
The separate multilib port below supports the full collection. A successful
build or instruction-oracle preflight is not a full current-core circuit pass.

Once the selected workloads have built and their execution budgets have been
checked, a full collection can be attempted explicitly:

```sh
tools/rv32i-benchmarks/build-embench.sh all
python3 tools/benchmarks/run.py rv32i \
  --output build/experiments/embench-1.0 --timeout 3600 -- \
  --elf-dir build/rv32i-embench/artifacts --max-cycles 20000000 \
  --max-events 2000000000 --cores single,baseline,advanced \
  --warmups 0 --repetitions 1
```

Build and execution remain explicit: normal CTest never downloads or builds
the external suite.

The script:

- fetches the exact upstream commit recorded in `embench.lock`;
- compiles with pinned GCC using `-march=rv32i -mabi=ilp32`;
- supplies only reset, stack, board hooks, linker layout, and `tohost` verdict
  plumbing;
- leaves Embench's benchmark body, scale factor, warmup, and verifier intact;
- writes source and artifacts under `build/rv32i-embench`.

Run the resulting ELF through both behavioral CircuitSim cores without
retaining a visualizer timeline:

```bash
build/rv32i_benchmark_elf \
  build/rv32i-embench/artifacts/crc32/crc32.elf \
  0x3ffc0 15000000 both
```

The `single-cycle`, `five-stage`, and `both` core selections are available.
This is an instruction/cycle experiment, not an official wall-clock Embench
score: CircuitSim's clock delay units are educational normalized delays, not a
calibrated MHz value.

## Full-suite multilib port

`build-embench-multilib.py` preserves the historical CRC32 builder and produces
a **separate ELF cohort**. The historical Docker toolchain's bundled libraries
are RV64; the full suite needs RV32I `libgcc`, newlib and `libm`, including
software multiplication/division and floating-point routines.

The official [xPack GCC 12.4.0-2 release](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v12.4.0-2)
provides `rv32i/ilp32` multilibs. Download its Linux x64 archive into
`build/toolchains/`, verify it against `embench-multilib.lock.json`, and extract
there. Do not substitute a different compiler silently when comparing results.

```sh
python3 tools/rv32i-benchmarks/build-embench-multilib.py \
  --compiler build/toolchains/xpack-riscv-none-elf-gcc-12.4.0-2/bin/riscv-none-elf-gcc \
  --output build/rv32i-embench-multilib/artifacts
```

The source checkout is the unchanged pinned `build/rv32i-embench/source`.
All 19 sources build serially under a 512 MiB per-process address-space cap.
`builds.json` records compiler identity, each command, ELF hash and build status.
The linker keeps the same 256 KiB address space and 16 KiB stack. Embench keeps
its own heap, scale factors, `CPU_MHZ=1`, one warmup, and verification routines.
The newlib `_sbrk` stub rejects OS-style heap requests; `_exit` reports failure,
so only return from the benchmark verifier can report success.

Use the [background campaign](../benchmarks/README.md#full-embench-background-run)
for the complete matrix. The long suite is behavioral circuit execution with
per-instruction checking, not the fast instruction oracle alone. Large structural
runs remain a separate, explicit choice.

### Reduced-work middle profile

Use the same builder with `--profile middle` and a separate output such as
`build/rv32i-embench-middle/artifacts`. The suite definition in
`embench-middle.json` selects CRC32, SHA-256 and SGLIB. `embench/middle.c`
includes the unmodified source translation unit and calls its private
`benchmark_body(1)` followed by the original verifier. No input or algorithm
is shortened; only outer timing repetitions and warmup are removed.

The distinct `embench-middle-*.elf` names and `builds.json` profile prevent
confusion with full results. The default full build is unchanged. The common
campaign preset `compiled-middle` performs complete reference/behavioral pairs;
see [the campaign instructions](../benchmarks/README.md#middle-scale-compiled-equivalence).
