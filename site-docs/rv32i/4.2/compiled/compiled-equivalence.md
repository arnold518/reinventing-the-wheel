# Compiled C Equivalence Fixture

[Compiled workload catalogue](index.md) · [Fidelity equivalence](../verification/equivalence.md)

## Purpose

Check that compiler-generated stack, call/return, load/store, branch, and wraparound behavior matches between the reference datapath and compact model of each core.

## Setup

| Property | Value |
| --- | --- |
| Source/image | `tests/fixtures/rv32i/differential/compiled.c` and `compiled.elf` |
| Compiler | Pinned GCC 16.1.0, RV32I/ilp32, -O2, no linker relaxation |
| Input | Four volatile unsigned words: 7, 0xffffffff, 0x80000000, 0x5a5a5a5a |
| Startup | Shared bare-metal stack/main/tohost bridge |
| Verification | Full completion, not a prefix |

## Implementation

```cpp
unsigned result = 0;
for (unsigned value : values) {
    if (value & 1) result += value;
    else result ^= value;
}
return result;
```

The actual C uses a non-inlined function and volatile array accesses. This simplified loop describes its behavior; the checked-in disassembly records the real generated instructions.

## Expected result

| Step | Input | Operation | Accumulator |
| --- | --- | --- | --- |
| 1 | 0x00000007 | Add odd value | 0x00000007 |
| 2 | 0xffffffff | Add with 32-bit wraparound | 0x00000006 |
| 3 | 0x80000000 | XOR even value | 0x80000006 |
| 4 | 0x5a5a5a5a | XOR even value | 0xda5a5a5c |

`main` returns zero only for this result. Startup converts success to tohost=1 and failure to 3. The test additionally compares settled cycle traces and final signatures, not only that verdict.

## Running and rebuilding

```sh
build/rv32i_experiment --core advanced \
  --elf tests/fixtures/rv32i/differential/compiled.elf \
  --period 4000 --allocation-limit-mib 512 --max-cycles 1024
bash tools/rv32i-benchmarks/build-equivalence.sh
```

Rebuilding is optional for ordinary tests. The script uses one CPU and 512 MiB; update the ELF, disassembly, and SHA-256 together if source or compiler changes. This project-owned fixture is **not** an Embench program.

## Source and tests

`RV32IDifferentialTest` runs the complete image on single-cycle, baseline, and advanced cores in both profiles: three paired cases. The [equivalence reference](../verification/equivalence.md) describes what is compared and what is deliberately ignored.
