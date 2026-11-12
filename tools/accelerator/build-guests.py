#!/usr/bin/env python3
"""Build the chapter 6 guest programs into tests/fixtures/accelerator/.

    python3 tools/accelerator/build-guests.py [--compiler PATH]

Each workload (matmul, mlp, cnn) is built for the CPU alone and for
MatrixUnit<4>, <8> and <16>; system.h comes from systems/accelerator-machine.json
(regenerate it with `system_header accelerator-machine` after a register change).
Writes <workload>-<cpu|nN>.elf, a disassembly of each, and manifest.json with
the compiler, flags and SHA-256 of every source and ELF.
"""
import argparse
import hashlib
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GUEST = os.path.join(ROOT, "tools", "accelerator", "guest")
RV32IM = os.path.join(ROOT, "tools", "rv32im", "guest")
OUT = os.path.join(ROOT, "tests", "fixtures", "accelerator")
DEFAULT_GCC = os.path.join(ROOT, "build", "toolchains", "xpack-riscv-none-elf-gcc-12.4.0-2", "bin", "riscv-none-elf-gcc")
FLAGS = ["-march=rv32im", "-mabi=ilp32", "-msmall-data-limit=0", "-mno-relax", "-O2", "-ffreestanding", "-fno-builtin",
         "-fno-tree-loop-distribute-patterns", "-fno-stack-protector", "-nostdlib", "-nostartfiles",
         "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
         "-Wl,--gc-sections", "-Wl,--no-relax", "-Wl,--build-id=none"]
WORKLOADS = {"matmul": 1, "mlp": 2, "cnn": 3, "demo": 4}
# The demo (the machine scenario) is built for MatrixUnit<4> and the CPU only.
MACHINES = {"demo": ("cpu", "n4")}
# Images per run (simulation time): two with the unit (also run as one
# batch), one on the CPU alone.
UNIT_IMAGES = 2
CPU_IMAGES = {"mlp": 1, "cnn": 1}


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default=DEFAULT_GCC)
    args = parser.parse_args()
    objdump = args.compiler[:-3] + "objdump"
    os.makedirs(OUT, exist_ok=True)
    sources = [os.path.join(RV32IM, "start.S"), os.path.join(GUEST, "workloads.c"), os.path.join(GUEST, "matrix.c")]
    builds = {}
    for name, number in WORKLOADS.items():
        for machine in MACHINES.get(name, ("cpu", "n4", "n8", "n16")):
            defines = [f"-DWORKLOAD={number}"]
            if machine == "cpu":
                if name in CPU_IMAGES:
                    defines.append(f"-DIMAGES_RUN={CPU_IMAGES[name]}")
            else:
                defines.append(f"-DMATRIX_N={machine[1:]}")
                if name in CPU_IMAGES:
                    defines.append(f"-DIMAGES_RUN={UNIT_IMAGES}")
            elf = os.path.join(OUT, f"{name}-{machine}.elf")
            command = [args.compiler, *FLAGS, *defines, "-I", GUEST, "-T", os.path.join(RV32IM, "link.ld"),
                       *sources, "-lgcc", "-o", elf]
            subprocess.run(command, check=True)
            disassembly = subprocess.run([objdump, "-d", "--no-show-raw-insn", elf], check=True, capture_output=True, text=True).stdout
            open(elf[:-4] + ".disasm.txt", "w").write(disassembly)
            builds[os.path.basename(elf)] = {"defines": defines, "sha256": sha(elf)}
    version = subprocess.run([args.compiler, "--version"], check=True, capture_output=True, text=True).stdout.splitlines()[0]
    inputs = sources + [os.path.join(GUEST, f) for f in sorted(os.listdir(GUEST)) if f.endswith(".h")] + [os.path.join(RV32IM, "link.ld")]
    manifest = {"compiler": version, "flags": FLAGS, "sources": {os.path.relpath(p, ROOT): sha(p) for p in inputs}, "elfs": builds}
    json.dump(manifest, open(os.path.join(OUT, "manifest.json"), "w"), indent=1)
    print(f"built {len(builds)} programs into {os.path.relpath(OUT, ROOT)}")


if __name__ == "__main__":
    main()
