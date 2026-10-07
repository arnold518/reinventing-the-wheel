#!/usr/bin/env bash
# Rebuild the small checked-in RV32I C fixture with the pinned Embench compiler.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
source "${script_dir}/embench.lock"
fixture=tests/fixtures/rv32i/differential
docker run --rm --memory 512m --cpus 1 --user "$(id -u):$(id -g)" \
    -e EXPECTED_GCC_VERSION="${TOOLCHAIN_GCC_VERSION}" \
    -v "${repo_root}:/workspace" -w /workspace "${TOOLCHAIN_IMAGE}" sh -euc '
        prefix=/opt/riscv/bin/riscv64-unknown-elf
        test "$("${prefix}-gcc" -dumpfullversion)" = "${EXPECTED_GCC_VERSION}"
        fixture=tests/fixtures/rv32i/differential
        "${prefix}-gcc" -march=rv32i -mabi=ilp32 -O2 -fno-builtin \
            -ffunction-sections -fdata-sections -nostdlib -nostartfiles \
            -Wl,--build-id=none,--no-relax,--gc-sections \
            -Wl,-T,tools/rv32i-benchmarks/embench/link.ld \
            tools/rv32i-benchmarks/embench/start.S "${fixture}/compiled.c" \
            -o "${fixture}/compiled.elf"
        "${prefix}-objdump" -d "${fixture}/compiled.elf" > "${fixture}/compiled.disasm.txt"
    '
(cd "${repo_root}/${fixture}" && sha256sum compiled.elf > compiled.elf.sha256)
