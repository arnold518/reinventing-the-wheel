# Small compiled equivalence workload

`compiled.c` exercises an actual compiler-generated stack frame, a non-inlined
function call and return, loads/stores, conditional branches, and 32-bit wraparound.
Its reduction must produce `0xda5a5a5c`; `main` returns zero only on success.
The shared bare-metal startup publishes `tohost = 1` on success or `3` on failure.
This is a project-owned C test, **not** an Embench benchmark.

`compiled.elf` and its SHA-256 checksum are checked in so ordinary tests need
neither Docker nor a cross-compiler. `compiled.disasm.txt` shows the executed
RV32I code. Rebuild from the repository root:

```sh
bash tools/rv32i-benchmarks/build-equivalence.sh
```

The script uses the immutable compiler image and GCC version from
`tools/rv32i-benchmarks/embench.lock`, the existing RV32I startup/linker script,
`-march=rv32i -mabi=ilp32 -O2`, and no standard library or linker relaxation.
Compilation is restricted to one CPU and 512 MiB. Update the ELF, disassembly
and checksum together when changing the C source or toolchain.

`RV32IDifferentialTest` runs this **complete** workload on all three cores, in
both the structural-datapath reference profile and the compact behavioral
profile. It compares their settled cycle traces and final benchmark signatures.
The same test also generates short seeded ELF instruction sequences in a
temporary directory; those are assembler-encoded tests, not compiled C.
