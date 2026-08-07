# External RV32I benchmarks

This directory contains the small CircuitSim port layer and reproducible build
tools for established upstream benchmarks. Upstream source and generated ELF
files stay under `build/`; they are not copied into the repository.

## Embench 1.0

Build the pinned, unmodified Embench CRC32 workload as a static bare-metal
RV32I ELF:

```bash
tools/rv32i-benchmarks/build-embench.sh crc32
```

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
