# RV32I External Validation Report

Last updated: 2026-07-29

## Result

CircuitSim now has an additive external-validation boundary for the
single-cycle RV32I system. It does not replace the 16 internal program
scenarios, change CPU execution semantics, or add a dependency on external
tools to normal CTest runs.

The first committed fixture has passed three independent paths:

1. Sail 0.13 executed the externally assembled ELF and reported `SUCCESS`
   through HTIF.
2. CircuitSim's behavioral RV32I system observed `tohost=1`.
3. CircuitSim's structural RV32I system observed `tohost=1` after the same
   instruction count.

The focused CTest result was:

```text
RV32IElfLoaderTest                  passed
RV32IExternalValidationSmokeTest   passed
2/2 passed in 15.54 seconds
```

The final serial repository regression passed:

```text
129/129 passed in 637.57 seconds
```

The generic runner also reported:

```text
PASS self-check.elf fidelity=both tohost=1 instructions=19
```

A fresh run of the 16 project program scenarios reported:

```text
16/16 passed in 326.73 seconds
```

The pinned official ACT4 `I` tier is now generated and has completed its
reference/behavioral pass:

```text
generated ACT4 ELFs:       39
Sail 0.13:                 39/39 passed
CircuitSim behavioral:     39/39 passed
retired instructions:      104,570
```

One fully structural ACT4 test was also measured:

```text
I-nop-00.elf:              passed
retired instructions:      184
elapsed:                   159.21 seconds
maximum resident memory:   1,133,508 KiB
```

At that measured rate, a serial run of all 104,570 instructions would take
about 25 hours. The host had only about 2.2 GiB available memory and exhausted
swap after the measured run, so a parallel or unbounded full-structural sweep
was not started. This is a documented resource boundary, not a passing result.

## What Was Added

### Strict ELF32 loader

`RV32IElfImage` accepts only the deliberately small bare-metal contract that
the current CPU can execute:

- ELF32, little-endian, version 1.
- Static `ET_EXEC` image for `EM_RISCV`.
- Four-byte-aligned entry point inside an executable `PT_LOAD` segment.
- Identical virtual and physical load addresses.
- Valid power-of-two segment alignment.
- File size no larger than memory size.
- Non-overlapping load segments with no 32-bit address overflow.
- Zero-filled `p_memsz - p_filesz` tail.
- Every segment and the entry point inside the existing 256 KiB memory.

Malformed-image tests cover the important rejection paths. General dynamic
ELF loading, relocations, shared objects, and operating-system loading are
intentionally outside this contract.

### Harvard image loading

The current system has separate instruction and data memories. Each ELF
`PT_LOAD` segment is mirrored into both initial memories:

```text
ELF PT_LOAD segment
        |-- initial instruction memory
        `-- initial data memory
```

This preserves the existing system. Instruction fetches see text, while loads
can see ELF data and constants. Runtime stores still change only data memory.
Self-modifying code is therefore not supported.

The common `RV32ISystemProgramAccess` capability gained
`loadInstructionBytes()`. Both system fidelities already had that operation;
the change only exposes it through their shared contract.

### External verdict

The fixture is self-checking. CircuitSim does not use
`RV32IInstructionOracle` to decide whether it passed:

- low word `1` at `tohost`: pass
- low word `3` at `tohost`: fail
- neither value before the instruction limit: timeout
- halt or trap before a verdict: failure

The runner learns the verdict by observing data-memory write history. It does
not read internal gates or cast the root to a concrete structural/behavioral
system class.

### Reusable runner

The registered CTest is one logical scenario:

```text
RV32IExternalValidationSmokeTest
```

Inside that scenario, the same ELF is executed once with each root fidelity.
There are no separate structural and behavioral CTest names.

Any compatible ELF can also be checked with:

```bash
cmake --build build -j"$(nproc)" --target rv32i_validate_elf
build/rv32i_validate_elf program.elf 0x3ffc0 100000
```

The remaining arguments are the `tohost` address, maximum instruction count,
and optional `behavioral`, `structural`, or `both` selection. The default is
`both`; the complete ACT4 set can be screened with the faster behavioral root
before its structural run.

## Committed Smoke Fixture

Source and provenance are under
`tests/fixtures/rv32i/external-smoke/`:

- `self-check.S`
- `link.ld`
- `self-check.elf`
- `self-check.elf.sha256`
- `self-check.readelf.txt`

Contract:

- reset/entry address: `0x00000000`
- `tohost`: `0x0003ffc0`
- ISA passed to the assembler: `-march=rv32i -mabi=ilp32`
- no standard library, startup files, or linker build ID
- linker relaxation disabled

The committed binary was built in the digest-pinned container
`debian@sha256:7b140f374b289a7c2befc338f42ebe6441b7ea838a042bbd5acbfca6ec875818`
with Debian's `riscv64-unknown-elf-gcc 12.2.0`. Rebuild it without installing
anything on the host:

```bash
fixture_uid="$(id -u)"
fixture_gid="$(id -g)"
docker run --rm \
  -e FIXTURE_UID="$fixture_uid" \
  -e FIXTURE_GID="$fixture_gid" \
  -v "$PWD:/workspace" -w /workspace \
  debian@sha256:7b140f374b289a7c2befc338f42ebe6441b7ea838a042bbd5acbfca6ec875818 \
  sh -lc '
    apt-get update &&
    apt-get install -y --no-install-recommends \
      gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf &&
    tools/rv32i-validation/generate-smoke-fixture.sh &&
    chown "$FIXTURE_UID:$FIXTURE_GID" \
      tests/fixtures/rv32i/external-smoke/self-check.elf \
      tests/fixtures/rv32i/external-smoke/self-check.elf.sha256 \
      tests/fixtures/rv32i/external-smoke/self-check.readelf.txt
  '
```

After rebuilding:

```bash
(cd tests/fixtures/rv32i/external-smoke &&
 sha256sum -c self-check.elf.sha256)
```

Sail 0.13 independently produced:

```text
HTIF located at 0x3ffc0
htif[0x00003FFC0] <- 0x00000001
htif[0x00003FFC4] <- 0x00000000
SUCCESS
```

## ACT4 Tier

`tools/rv32i-validation/` contains a pinned ACT4 workflow:

- upstream repository and commit lock
- required GCC/toolchain and Sail versions
- a UDB RV32I configuration
- the 256 KiB, reset-at-zero linker map
- `tohost` pass/fail macros
- a deterministic Sail-config narrowing script
- an ELF generation script whose default output is under `build/`

Pinned upstream source:

```text
repository: https://github.com/riscv/riscv-arch-test.git
commit:     3548427a83bd7aa3a813d0b2b622e8dbc691c498
toolchain:  upstream release 2026.07.15 / GCC 16.1.0 / Binutils 2.46
Sail:       0.13
```

The UDB configuration was validated with UDB 0.1.14 in the same
digest-pinned Ubuntu base used by upstream ACT4. The narrowed Sail config was
also validated by Sail 0.13 and used for the successful smoke execution.

Generate official self-checking `I` ELFs only when the pinned ACT4 toolchain
is intentionally available:

```bash
tools/rv32i-validation/generate-act4-fixtures.sh
```

The script:

- refuses unsafe broad output paths;
- checks every required tool before writing;
- rejects the wrong ACT4 commit, Sail template checksum, exact GCC/Binutils
  versions, or Sail version;
- selects `EXTENSIONS=I` and disables privileged tests;
- writes generated sources and ELFs under `build/rv32i-act4`;
- creates a checksum manifest.

The host does not install the ACT4 toolchain globally. Generation and Sail
execution used the upstream image for the pinned commit:

```text
ghcr.io/riscv/act4-build
image digest:
sha256:b84781fdf5bf0b89348deb574b39e9f8184f815cb01dc7931a4724050f2e91e5
```

All 39 generated ELF checksums passed. Sail and the behavioral CircuitSim
system both reported `SUCCESS`/`tohost=1` for every ELF. The structural tier
has a measured passing sample but not a complete 39-test run.

## Scope Boundary

This validation targets the unprivileged RV32I instruction behavior supported
by the project. It does not claim:

- RISC-V privileged architecture certification;
- standard `ECALL`/`EBREAK` environment behavior;
- interrupts, timers, CSRs, or operating-system execution;
- compressed, multiply/divide, atomic, floating-point, or vector extensions;
- unified-memory or self-modifying-code behavior.

The existing project-specific halt/trap tests remain responsible for
`ECALL`, `EBREAK`, illegal instructions, and memory faults. ACT4 remains an
additional answer-sheet check, not a replacement for those tests.

## Remaining Work

1. Add a deliberately headless/low-history structural validation mode, without
   weakening simulation semantics or visualizer history.
2. Re-measure memory and instruction throughput in that mode.
3. Run all 39 ACT4 ELFs through the fully structural profile when the resource
   budget is safe.
4. Classify any mismatch as loader, environment-contract, behavioral,
   structural, or specification-policy work.
5. Commit generated fixtures only if their size and provenance have been
   reviewed; normal CTest remains independent of them.
