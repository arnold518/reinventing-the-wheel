#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
config_source="${script_dir}/act4-config"
lock_file="${script_dir}/act4.lock"
requested_output="${1:-${repo_root}/build/rv32i-act4}"
output_root="$(realpath -m -- "${requested_output}")"

# The lock file is repository-owned and contains assignments only.
# shellcheck source=act4.lock
source "${lock_file}"

case "${output_root}" in
    "/"|"${repo_root}"|"${repo_root}/tests"|"${repo_root}/tests/"*|\
    "${repo_root}/visualizer"|"${repo_root}/visualizer/"*|\
    "${repo_root}/visualizer-v2"|"${repo_root}/visualizer-v2/"*)
        echo "Refusing unsafe ACT4 output directory: ${output_root}" >&2
        exit 2
        ;;
esac

for tool in git make python3 sha256sum \
    riscv64-unknown-elf-gcc \
    riscv64-unknown-elf-objdump \
    sail_riscv_sim \
    mise; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Required ACT4 tool not found: ${tool}" >&2
        exit 2
    fi
done

compiler_version="$(riscv64-unknown-elf-gcc -dumpfullversion)"
binutils_version="$(
    riscv64-unknown-elf-objdump --version | sed -n '1s/.* //p'
)"
sail_version="$(sail_riscv_sim --version 2>&1 | sed -n '1p')"
if [[ "${compiler_version}" != "${ACT4_GCC_VERSION}" ]]; then
    echo "ACT4 requires GCC ${ACT4_GCC_VERSION}; found ${compiler_version}" >&2
    echo "Expected upstream toolchain release: ${ACT4_TOOLCHAIN_RELEASE}" >&2
    exit 3
fi
if [[ "${binutils_version}" != "${ACT4_BINUTILS_VERSION}" ]]; then
    echo "ACT4 requires Binutils ${ACT4_BINUTILS_VERSION}; found ${binutils_version}" >&2
    echo "Expected upstream toolchain release: ${ACT4_TOOLCHAIN_RELEASE}" >&2
    exit 3
fi
if [[ "${sail_version}" != *"${ACT4_SAIL_VERSION}"* ]]; then
    echo "ACT4 requires Sail ${ACT4_SAIL_VERSION}; found ${sail_version}" >&2
    exit 3
fi

mkdir -p -- "${output_root}"
source_dir="${output_root}/source"
config_dir="${output_root}/config"
work_dir="${output_root}/work"

if [[ -e "${source_dir}" && ! -d "${source_dir}/.git" ]]; then
    echo "Refusing to initialize over existing non-git path: ${source_dir}" >&2
    exit 4
fi
if [[ ! -d "${source_dir}/.git" ]]; then
    git init -q "${source_dir}"
    git -C "${source_dir}" remote add origin "${ACT4_REPOSITORY}"
    git -C "${source_dir}" fetch --depth 1 origin "${ACT4_COMMIT}"
    git -C "${source_dir}" checkout -q --detach FETCH_HEAD
fi

actual_commit="$(git -C "${source_dir}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${ACT4_COMMIT}" ]]; then
    echo "ACT4 worktree is not at the pinned commit." >&2
    echo "Expected ${ACT4_COMMIT}, found ${actual_commit}" >&2
    exit 4
fi

mkdir -p -- "${config_dir}" "${work_dir}"
for name in \
    circuitsim-rv32i.yaml \
    link.ld \
    rvmodel_macros.h \
    test_config.yaml; do
    install -m 0644 \
        "${config_source}/${name}" \
        "${config_dir}/${name}"
done

template_sail_config="${source_dir}/config/sail/sail-RVI20U32/sail.json"
actual_template_sha="$(
    sha256sum "${template_sail_config}" | awk '{print $1}'
)"
if [[ "${actual_template_sha}" != "${ACT4_SAIL_CONFIG_SHA256}" ]]; then
    echo "Pinned ACT4 Sail config checksum mismatch." >&2
    exit 4
fi
python3 "${script_dir}/prepare_act4_sail_config.py" \
    "${template_sail_config}" \
    "${config_dir}/sail.json"

(
    cd -- "${source_dir}"
    CONFIG_FILES="${config_dir}/test_config.yaml" \
    WORKDIR="${work_dir}" \
    EXTENSIONS=I \
    FAST=1 \
    make elfs
)

elf_dir="${work_dir}/circuitsim-rv32i/elfs"
if [[ ! -d "${elf_dir}" ]]; then
    echo "ACT4 completed without an ELF output directory." >&2
    exit 5
fi

manifest="${output_root}/elfs.sha256"
(
    cd -- "${elf_dir}"
    find . -type f -name '*.elf' -print0 \
        | sort -z \
        | xargs -0 -r sha256sum
) >"${manifest}"

elf_count="$(
    find "${elf_dir}" -type f -name '*.elf' | wc -l
)"
if [[ "${elf_count}" -eq 0 ]]; then
    echo "ACT4 produced no RV32I ELFs." >&2
    exit 5
fi

echo "Generated ${elf_count} pinned ACT4 RV32I ELFs in ${elf_dir}"
echo "Checksums: ${manifest}"
echo "Validate one ELF with:"
echo "  ${repo_root}/build/rv32i_validate_elf <elf> 0x3ffc0 100000 behavioral"
