# RV32I External Benchmark Report

Date: 2026-08-03  
Branch: `rv32i-pipeline-performance`

## Result

CircuitSim executed the unmodified full CRC32 workload from the online
Embench 1.0 suite on both behavioral RV32I cores. Both executions reached the
benchmark's own success verdict with the same retired instruction count.

| Measurement | Single-cycle | Five-stage |
|---|---:|---:|
| Upstream verifier | pass | pass |
| Retired instructions | 6,130,443 | 6,130,443 |
| Hardware cycles | 6,130,443 | 7,181,789 |
| CPI | 1.000000 | 1.171496 |
| Load-use stalls | 0 | 0 |
| Memory-ready stalls | 0 | 0 |
| Data-port stalls | 0 | 0 |
| Pipeline flushes | 0 | 525,672 |
| Average pipeline occupancy | n/a | 87.19% |
| Processed simulator events | 61,304,450 | 436,797,039 |
| Maximum event queue | 9 | 24 |

The pipeline uses 17.15% more cycles because control redirects flush younger
work. This compiler-generated CRC32 loop has no immediately dependent loads,
so it produces no load-use bubbles with the current forwarding network.

Using the previously measured normalized clock periods of 189 delay units for
the single-cycle system and 108 for the five-stage system:

| Model | Cycles x clock period | Relative speed |
|---|---:|---:|
| Single-cycle | 1,158,653,727 | 1.000x |
| Five-stage | 775,633,212 | **1.494x** |

Thus this workload shows a **49.4% modeled hardware speedup** for the pipeline.
These are educational normalized delay units, not nanoseconds or a measured
FPGA/ASIC clock.

## Upstream Provenance

- Suite: Embench 1.0
- Repository: <https://github.com/embench/embench-iot.git>
- Tag: `embench-1.0`
- Commit: `0466a18e4f6b47e19598d7c6ba72916d54b68f65`
- Workload: `src/crc32/crc_32.c`
- Toolchain: GCC 16.1.0 from the pinned ACT4 toolchain image
- ISA/ABI: `-march=rv32i -mabi=ilp32`
- ELF SHA-256:
  `0008a0e83ab7c1892e88d82d84d068d5e0b19b5b7297c063458476e3e90d9e69`

The repository-owned port supplies only `_start`, stack placement, empty board
hooks, the 256 KiB linker map, and a `tohost` pass/fail adapter. Embench's
`main.c`, CRC32 source, benchmark scale factor of 170, warmup, and
`verify_benchmark()` are not patched or shortened.

The generated image contains 1,496 bytes of text, 16 bytes of data, and four
bytes of BSS. It is a static ELF32 little-endian RISC-V executable with entry
address zero.

## Independent Check

Sail 0.13 executed the same ELF and reported `SUCCESS` after 6,130,444
instructions. CircuitSim reports one fewer instruction because its runner
stops when the low `tohost=1` word becomes visible; Sail continues through the
following high-word host-interface store. This one-instruction reporting
difference occurs after the benchmark verifier has completed.

## Headless Execution

The benchmark exposed three independent sources of retained visualization
history:

1. simulator wire and pin timelines;
2. `Memory64Kx32` byte/write timelines;
3. external-runner checkpoint strings.

All now have an explicit headless path. Functional state, event ordering, and
performance counters remain active, but timeline APIs reject time travel or
history queries instead of returning incomplete data. Normal tests and the
visualizer retain history by default.

The final optimized-host run completed both cores in 318.12 seconds with
35.9 MiB peak RSS:

- single-cycle host time: 21.22 seconds;
- five-stage host time: 296.90 seconds.

The five-stage model is much slower as a host program because it processed
about 7.1 times as many events. Host simulation speed and modeled hardware
speed are separate results.

## Reproduction

```bash
tools/rv32i-benchmarks/build-embench.sh crc32

cmake -S . -B build-benchmark-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark-release -j2 --target rv32i_benchmark_elf

build-benchmark-release/rv32i_benchmark_elf \
  build/rv32i-embench/artifacts/crc32/crc32.elf \
  0x3ffc0 15000000 both
```

The standard Debug build produces identical architectural and pipeline
metrics. A pinned ACT4 `I-add` ELF was used to verify this before the full
run: both builds produced 3,368 instructions, 4,561 five-stage cycles, 393
load-use stalls, and 399 flushes.

## Verification

- Full serial CTest suite: **175/175 passed** in 988.78 seconds.
- The suite includes the external benchmark runner smoke test, simulator and
  memory headless-history contracts, every registered RV32I program on both
  cores, and the visualizer bindings/profile store tests.
- The full Embench CRC32 ELF passed on both cores and independently under
  Sail, as described above.

## Scope and Limits

This is a real online benchmark program, but it is only one of Embench 1.0's
19 programs. It is not an official aggregate Embench score. CircuitSim does
not yet model a calibrated clock frequency, caches, variable memory latency,
or a reference board, so reporting an Embench speed score would be
misleading.

The six-million-instruction run uses each core's behavioral fidelity. Smaller
program, ACT4, block, and profile-equivalence tests remain responsible for
proving the structural versions against the same contracts. A fully
structural six-million-instruction run is not currently practical in the
event-driven gate simulator.

Next, add several more integer-only Embench workloads and report per-program
cycle counts and a geometric summary. The full 19-program score should wait
until unsupported compiler-runtime requirements are classified and the
timing model has a documented reference platform.
