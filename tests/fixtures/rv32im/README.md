# RV32IM compiler smoke

`smoke.c` uses volatile signed/unsigned inputs, records eight arithmetic
results and returns zero only when all expected values match. Startup sets
SP=4096, calls main and executes the existing educational EBREAK stop.
The ELF loader initializes the BSS from its load segment. No C library,
floating point, compressed instructions, CSR or privileged runtime is linked.

Rebuild from the repository root using the recorded xPack GCC 12.4.0 toolchain:

```sh
python3 tools/rv32im/build-smoke.py --compiler /path/to/riscv-none-elf-gcc
```

The script checks the final text for 32-bit RV32IM instructions and the six M
mnemonics this compiler selects. It emits MUL/MULHU with sign correction for
signed high products; explicit circuit/core cases separately cover MULH and
MULHSU. `toolchain.txt`, `smoke.disasm.txt` and `smoke.elf.sha256` identify the
build. Rebuilding this fixture does not change any RV32I compiler path.

`RV32IMCompiledProgramTest` loads this actual ELF into the new simulated core,
compares every retirement/register state and final memory to the independent
oracle, requires the C result x10=0 at EBREAK and checks setup violations.
The delayed host responder is test equipment; this fixture does not establish
execution through simulated device hardware or a full game platform.
