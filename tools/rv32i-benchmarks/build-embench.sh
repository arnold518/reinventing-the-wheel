#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"

# The lock file is repository-owned and contains assignments only.
# shellcheck source=embench.lock
source "${script_dir}/embench.lock"

benchmark="${1:-crc32}"
output_root="${2:-${repo_root}/build/rv32i-embench}"
output_root="$(realpath -m -- "${output_root}")"
source_root="${output_root}/source"
artifact_root="${output_root}/artifacts"

case "${benchmark}" in
    ""|*[!a-z0-9-]*)
        echo "Invalid Embench benchmark name: ${benchmark}" >&2
        exit 2
        ;;
esac

case "${output_root}" in
    "/"|"${repo_root}"|"${repo_root}/tests"|"${repo_root}/tests/"*|\
    "${repo_root}/visualizer"|"${repo_root}/visualizer/"*|\
    "${repo_root}/visualizer-v2"|"${repo_root}/visualizer-v2/"*)
        echo "Refusing unsafe Embench output directory: ${output_root}" >&2
        exit 2
        ;;
esac
case "${output_root}" in
    "${repo_root}/"*) ;;
    *)
        echo "Embench output must remain under the repository: ${output_root}" >&2
        exit 2
        ;;
esac
output_relative="${output_root#${repo_root}/}"

for tool in git docker sha256sum; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Required tool not found: ${tool}" >&2
        exit 2
    fi
done

if [[ -e "${source_root}" && ! -d "${source_root}/.git" ]]; then
    echo "Refusing to initialize over non-git source path: ${source_root}" >&2
    exit 3
fi
if [[ ! -d "${source_root}/.git" ]]; then
    mkdir -p -- "${output_root}"
    git init -q "${source_root}"
    git -C "${source_root}" remote add origin "${EMBENCH_REPOSITORY}"
    git -C "${source_root}" fetch --depth 1 origin "${EMBENCH_COMMIT}"
    git -C "${source_root}" checkout -q --detach FETCH_HEAD
fi

actual_commit="$(git -C "${source_root}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${EMBENCH_COMMIT}" ]]; then
    echo "Expected Embench ${EMBENCH_COMMIT}; found ${actual_commit}" >&2
    exit 3
fi

benchmark_source="${source_root}/src/${benchmark}"
if [[ ! -d "${benchmark_source}" ]]; then
    echo "Unknown Embench benchmark: ${benchmark}" >&2
    exit 4
fi

mkdir -p -- "${artifact_root}/${benchmark}"
artifact_dir="${artifact_root}/${benchmark}"
repo_uid="$(id -u)"
repo_gid="$(id -g)"

docker run --rm \
    --user 0:0 \
    -e REPO_UID="${repo_uid}" \
    -e REPO_GID="${repo_gid}" \
    -e BENCHMARK="${benchmark}" \
    -e OUTPUT_RELATIVE="${output_relative}" \
    -e EXPECTED_GCC_VERSION="${TOOLCHAIN_GCC_VERSION}" \
    -v "${repo_root}:/workspace" \
    -w /workspace \
    "${TOOLCHAIN_IMAGE}" \
    sh -euc '
        toolchain_prefix=/opt/riscv/bin/riscv64-unknown-elf
        compiler="${toolchain_prefix}-gcc"
        readelf_tool="${toolchain_prefix}-readelf"
        size_tool="${toolchain_prefix}-size"
        actual_version="$(${compiler} -dumpfullversion)"
        if [ "${actual_version}" != "${EXPECTED_GCC_VERSION}" ]; then
            echo "Expected GCC ${EXPECTED_GCC_VERSION}; found ${actual_version}" >&2
            exit 5
        fi

        source_root=/workspace/${OUTPUT_RELATIVE}/source
        port_root=/workspace/tools/rv32i-benchmarks/embench
        artifact_root=/workspace/${OUTPUT_RELATIVE}/artifacts/${BENCHMARK}
        benchmark_sources="$(find "${source_root}/src/${BENCHMARK}" -maxdepth 1 -type f -name "*.c" -print | sort)"
        if [ -z "${benchmark_sources}" ]; then
            echo "Benchmark ${BENCHMARK} has no C sources" >&2
            exit 6
        fi

        ${compiler} \
            -march=rv32i -mabi=ilp32 -O2 \
            -ffunction-sections -fdata-sections \
            -fno-builtin -fno-common \
            -nostdlib -nostartfiles \
            -DHAVE_BOARDSUPPORT_H -DWARMUP_HEAT=1 \
            -I"${port_root}" -I"${source_root}/support" \
            -Wl,--build-id=none -Wl,--no-relax -Wl,--gc-sections \
            -Wl,-T,"${port_root}/link.ld" \
            "${port_root}/start.S" \
            "${port_root}/boardsupport.c" \
            "${source_root}/support/main.c" \
            "${source_root}/support/beebsc.c" \
            ${benchmark_sources} \
            -lgcc \
            -o "${artifact_root}/${BENCHMARK}.elf"

        ${readelf_tool} -h -l -s "${artifact_root}/${BENCHMARK}.elf" \
            >"${artifact_root}/${BENCHMARK}.readelf.txt"
        ${size_tool} "${artifact_root}/${BENCHMARK}.elf" \
            >"${artifact_root}/${BENCHMARK}.size.txt"
        chown -R "${REPO_UID}:${REPO_GID}" "${artifact_root}"
    '

(
    cd -- "${artifact_dir}"
    sha256sum "${benchmark}.elf" >"${benchmark}.elf.sha256"
)

echo "Built Embench ${EMBENCH_TAG}/${benchmark} from ${EMBENCH_COMMIT}"
echo "ELF: ${artifact_dir}/${benchmark}.elf"
sed -n '1,2p' "${artifact_dir}/${benchmark}.size.txt"
