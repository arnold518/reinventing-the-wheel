#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
fixture_dir="${repo_root}/tests/fixtures/rv32i/external-smoke"
compiler="${RV32I_GCC:-riscv64-unknown-elf-gcc}"
readelf_tool="${RV32I_READELF:-riscv64-unknown-elf-readelf}"

for tool in "${compiler}" "${readelf_tool}" sha256sum; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Required tool not found: ${tool}" >&2
        exit 2
    fi
done

temporary_dir="$(mktemp -d)"
trap 'rm -rf -- "${temporary_dir}"' EXIT

output_elf="${temporary_dir}/self-check.elf"
"${compiler}" \
    -march=rv32i \
    -mabi=ilp32 \
    -nostdlib \
    -nostartfiles \
    -Wl,--build-id=none \
    -Wl,--no-relax \
    -Wl,-T,"${fixture_dir}/link.ld" \
    "${fixture_dir}/self-check.S" \
    -o "${output_elf}"

"${readelf_tool}" -h -l -s "${output_elf}" \
    >"${temporary_dir}/self-check.readelf.txt"

entry="$("${readelf_tool}" -h "${output_elf}" \
    | awk '/Entry point address:/ {print $4}')"
tohost="$("${readelf_tool}" -s "${output_elf}" \
    | awk '$8 == "tohost" {print "0x" $2; exit}')"
if [[ "${entry}" != "0x0" || "${tohost}" != "0x0003ffc0" ]]; then
    echo "Unexpected fixture contract: entry=${entry}, tohost=${tohost}" >&2
    exit 3
fi

install -m 0644 "${output_elf}" "${fixture_dir}/self-check.elf"
install -m 0644 \
    "${temporary_dir}/self-check.readelf.txt" \
    "${fixture_dir}/self-check.readelf.txt"
(
    cd -- "${fixture_dir}"
    sha256sum self-check.elf >self-check.elf.sha256
)

echo "Generated ${fixture_dir}/self-check.elf"
"${compiler}" --version | sed -n '1p'
