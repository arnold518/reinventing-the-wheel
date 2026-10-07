#!/usr/bin/env python3
"""Build pinned Embench workloads using RV32I newlib/libm/libgcc.

Separate from the historical GCC 16 CRC32 port: these are a new ELF cohort.
The default full profile preserves all upstream repetitions and warmup.
The middle profile runs complete kernels once, without warmup, retaining the
original inputs/verifiers and explicitly distinct ELF names/provenance.
"""
import argparse
import hashlib
import json
import resource
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PORT = ROOT / "tools/rv32i-benchmarks/embench"


def cap_memory():
    resource.setrlimit(resource.RLIMIT_AS, (512 * 1024**2, 512 * 1024**2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--source", type=Path, default=ROOT / "build/rv32i-embench/source")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=("full", "middle"), default="full")
    args = parser.parse_args()
    compiler, source, output = args.compiler.resolve(), args.source.resolve(), args.output.resolve()
    lock = json.loads((PORT.parent / "embench-multilib.lock.json").read_text())
    source_lock = dict(line.split("=", 1) for line in (PORT.parent / "embench.lock").read_text().splitlines()
                       if line and not line.startswith("#"))
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != source_lock["EMBENCH_COMMIT"]:
        raise RuntimeError("Wrong Embench revision")
    if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip():
        raise RuntimeError("Embench source must be unchanged")
    version = subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip()
    multilib = subprocess.check_output([str(compiler), "-march=rv32i", "-mabi=ilp32",
                                        "-print-multi-directory"], text=True).strip()
    if version != lock["gcc_version"] or multilib != lock["multilib"]:
        raise RuntimeError(f"Wrong compiler/multilib: {version}, {multilib}")
    suite = json.loads((PORT.parent / "embench-middle.json").read_text()) if args.profile == "middle" else None
    names = {case["name"]: case for case in suite["cases"]} if suite else None
    directories = [source / "src" / name for name in names] if names else sorted((source / "src").iterdir())
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"schema": 1, "source_commit": commit, "compiler": str(compiler),
                "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
                "toolchain": lock, "profile": args.profile, "cpu_mhz": 1,
                "warmup_heat": suite["warmup_heat"] if suite else 1,
                "kernel_repetitions": suite["kernel_repetitions"] if suite else "upstream-local-scale",
                "suite": suite, "builds": []}
    for directory in directories:
        sources = sorted(directory.glob("*.c")) if directory.is_dir() else []
        if not sources:
            continue
        dest = output / directory.name
        dest.mkdir()
        elf = dest / (names[directory.name]["elf"] if names else directory.name + ".elf")
        benchmark_inputs = ([source / "support/beebsc.c", PORT / "middle.c"] if suite else
                            [source / "support/main.c", source / "support/beebsc.c", *sources])
        defines = ([f'-DRITW_EMBENCH_SOURCE="{names[directory.name]["source"]}"',
                    f'-DRITW_KERNEL_REPETITIONS={suite["kernel_repetitions"]}',
                    "-I" + str(source / "src"), "-save-temps=obj"] if suite else [])
        # Stable intermediate names keep the assembler's FILE symbol from
        # embedding a random ccXXXXXX.o name in the middle ELF. Full-profile
        # command lines and historical artifacts are deliberately unchanged.
        command = [str(compiler), "-march=rv32i", "-mabi=ilp32", "-O2",
                   "-ffunction-sections", "-fdata-sections", "-fno-builtin", "-fno-common",
                   "-nostdlib", "-nostartfiles", "-DHAVE_BOARDSUPPORT_H",
                   f'-DWARMUP_HEAT={manifest["warmup_heat"]}', *defines,
                   "-I" + str(PORT), "-I" + str(source / "support"),
                   "-Wl,--build-id=none", "-Wl,--no-relax", "-Wl,--gc-sections",
                   "-Wl,-T," + str(PORT / "link.ld"), str(PORT / "start.S"),
                   str(PORT / "boardsupport.c"), str(PORT / "syscalls.c"),
                   *map(str, benchmark_inputs),
                   "-Wl,--start-group", "-lm", "-lc", "-lgcc", "-lnosys",
                   "-Wl,--end-group", "-o", str(elf)]
        with (dest / "build.log").open("w") as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=300, preexec_fn=cap_memory)
        row = {"name": directory.name, "command": command, "exit": result.returncode,
               "elf": str(elf.relative_to(output))}
        if suite:
            row["upstream_source"] = names[directory.name]["source"]
            row["upstream_source_sha256"] = hashlib.sha256((source / "src" / row["upstream_source"]).read_bytes()).hexdigest()
        if not result.returncode:
            row["elf_sha256"] = hashlib.sha256(elf.read_bytes()).hexdigest()
        manifest["builds"].append(row)
        (output / "builds.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(directory.name, "built" if result.returncode == 0 else "FAILED", flush=True)
    return int(any(row["exit"] for row in manifest["builds"]) or
               (suite is not None and len(manifest["builds"]) != len(suite["cases"])))


if __name__ == "__main__":
    raise SystemExit(main())
