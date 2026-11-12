#!/usr/bin/env python3
"""Campaign 1 of chapter 6: the CPU alone against the CPU with the matrix unit.

    python3 tools/accelerator/run-campaign.py run [--jobs 2] [--only NAME...]
    python3 tools/accelerator/run-campaign.py table

`run` executes every configuration below with build-accel/accelerator_experiment
(skipping those with a result already) and writes one JSON file per run into
build-accel/campaign/. Each run goes into its own memory-capped user scope.
`table` checks every run's outputs against the references and prints the
Markdown tables of site-docs/accelerator/6.6.md.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "build-accel", "accelerator_experiment")
OUT = os.path.join(ROOT, "build-accel", "campaign")
WORKLOADS = ("matmul", "mlp", "cnn")
DRAM_RETENTION = 64_000_000_000


def configurations():
    runs = []
    def add(workload, matrix, memory, fast=True):
        program = f"{workload}-{'cpu' if matrix == 0 else 'n' + str(matrix)}"
        name = f"{program}-{memory}" + ("" if fast else "-plain")
        runs.append({"name": name, "program": program, "matrix": matrix, "memory": memory, "fast": fast})
    for workload in WORKLOADS:
        for matrix in (0, 4, 8, 16):          # array size, SRAM
            add(workload, matrix, "sram")
        for memory in ("ff", "dram"):         # memory, 16 x 16
            add(workload, 16, memory)
    for matrix in (0, 16):                    # the chapter-5 bus and iterative multiplier
        add("matmul", matrix, "sram", fast=False)
    return runs


def run_one(config):
    path = os.path.join(OUT, config["name"] + ".json")
    if os.path.exists(path):
        return config["name"], "kept"
    command = [os.path.join(ROOT, "tools", "accelerator", "capped.sh"), "1600", TOOL, "--program", config["program"],
               "--matrix", str(config["matrix"]), "--memory", config["memory"]]
    if config["fast"]:
        command += ["--array-multiplier", "--pipelined-bus"]
    if config["memory"] == "dram":
        # A real DRAM's 64 ms (tREF): the unit profile's 1 ms (10^9 ticks) is
        # shorter than the matrix-multiply program, and this DRAM has no refresh.
        command += ["--dram-retention", str(DRAM_RETENTION)]
    start = time.time()
    with open(path + ".log", "w") as log:
        done = subprocess.run(command, stdout=subprocess.PIPE, stderr=log, text=True, cwd=ROOT)
    if done.returncode != 0:
        return config["name"], f"failed ({done.returncode}), see {path}.log"
    result = json.loads(done.stdout.strip().splitlines()[-1])
    result["wall_seconds"] = time.time() - start
    json.dump(result, open(path, "w"), indent=1)
    return config["name"], f"{result['cycles']} cycles in {result['wall_seconds']:.0f} s"


def references():
    model = json.load(open(os.path.join(ROOT, "tests", "fixtures", "accelerator", "model.json")))
    return {"mlp": [v for row in model["expected_mlp"] for v in row],
            "cnn": [v for row in model["expected_cnn"] for v in row]}


def table():
    refs = references()
    rows = {}
    for config in configurations():
        path = os.path.join(OUT, config["name"] + ".json")
        if os.path.exists(path):
            rows[config["name"]] = (config, json.load(open(path)))
    problems = []
    for name, (config, r) in rows.items():
        if not r["halted"] or r["status"] != 0 or r["setup_violations"]:
            problems.append(f"{name}: status {r['status']}, violations {r['setup_violations']}")
        workload = config["program"].split("-")[0]
        if workload in refs:
            images = 1 if config["matrix"] == 0 else 2
            if r["outputs"][:10 * images] != refs[workload][:10 * images]:
                problems.append(f"{name}: scores differ from model.json")
    matmul_cpu = rows.get("matmul-cpu-sram")
    for name, (config, r) in rows.items():
        if config["program"].startswith("matmul") and matmul_cpu and r["outputs"] != matmul_cpu[1]["outputs"]:
            problems.append(f"{name}: matrix-multiply outputs differ from the CPU's")
    print("Checks:", "all runs halted cleanly with the reference outputs" if not problems else "; ".join(problems))

    print("\n### Array size (SRAM)\n")
    print("| Workload | CPU alone | 4 × 4 | 8 × 8 | 16 × 16 | Speedup at 16 × 16 |")
    print("| --- | ---: | ---: | ---: | ---: | ---: |")
    for workload in WORKLOADS:
        cells, base = [], None
        for matrix in (0, 4, 8, 16):
            key = f"{workload}-{'cpu' if matrix == 0 else 'n' + str(matrix)}-sram"
            if key not in rows:
                cells.append("—"); continue
            r = rows[key][1]
            value = sum(r["sections"][:3]) if workload == "matmul" else r["sections"][0]
            if matrix == 0: base = value
            cells.append(f"{value:,}")
        last = rows.get(f"{workload}-n16-sram")
        speed = "—"
        if base and last:
            v = sum(last[1]["sections"][:3]) if workload == "matmul" else last[1]["sections"][0]
            speed = f"{base / v:.1f}×"
        label = {"matmul": "Matrix multiply (3 layers)", "mlp": "MLP, one image", "cnn": "CNN, one image"}[workload]
        print(f"| {label} | " + " | ".join(cells) + f" | {speed} |")
    print("\n### Main memory (16 × 16)\n")
    print("| Workload | FF | SRAM | DRAM |")
    print("| --- | ---: | ---: | ---: |")
    for workload in WORKLOADS:
        cells = []
        for memory in ("ff", "sram", "dram"):
            key = f"{workload}-n16-{memory}"
            if key not in rows: cells.append("—"); continue
            r = rows[key][1]
            cells.append(f"{(sum(r['sections'][:3]) if workload == 'matmul' else r['sections'][0]):,}")
        print(f"| {workload} | " + " | ".join(cells) + " |")
    print("\n### Matrix-multiply layers (SRAM): cycles per layer\n")
    print("| Machine | 4 × 32 → 16 | 8 × 64 → 32 | 16 × 64 → 64 |")
    print("| --- | ---: | ---: | ---: |")
    for key, label in (("matmul-cpu-sram", "CPU alone"), ("matmul-n4-sram", "4 × 4"), ("matmul-n8-sram", "8 × 8"),
                       ("matmul-n16-sram", "16 × 16"), ("matmul-cpu-sram-plain", "CPU alone, chapter-5 bus and multiplier"),
                       ("matmul-n16-sram-plain", "16 × 16, chapter-5 bus and multiplier")):
        if key in rows:
            s = rows[key][1]["sections"]
            print(f"| {label} | " + " | ".join(f"{v:,}" for v in s[:3]) + " |")
    # What limits the unit: the edges the array, the shift and the activate
    # engines are busy (from the command list) against the measured layer.
    print("\n### Matrix-multiply layers (SRAM): array busy against measured\n")
    print("| Unit | Layer | Array, shift, activate edges | Measured cycles | Busy share |")
    print("| --- | --- | ---: | ---: | ---: |")
    shapes = ((4, 32, 16), (8, 64, 32), (16, 64, 64))
    for matrix in (4, 8, 16):
        key = f"matmul-n{matrix}-sram"
        if key not in rows: continue
        s = rows[key][1]["sections"]
        for (m, k, o), measured in zip(shapes, s[:3]):
            tiles, chunks = -(-o // matrix), -(-k // matrix)
            busy = tiles * chunks * (matrix + m + 2 * matrix - 1) + tiles * m
            print(f"| {matrix} × {matrix} | {m} × {k} → {o} | {busy:,} | {measured:,} | {100 * busy / measured:.0f} % |")
    print("\n### Runs\n")
    print("| Run | Cycles | Instructions | Unit commands | Host seconds |")
    print("| --- | ---: | ---: | ---: | ---: |")
    for name, (config, r) in sorted(rows.items()):
        print(f"| `{name}` | {r['cycles']:,} | {r['instructions']:,} | {r['commands']:,} | {r['run_seconds']:.0f} |")
    for name in ("mlp-n4-sram", "mlp-n8-sram", "mlp-n16-sram"):
        if name in rows:
            s = rows[name][1]["sections"]
            print(f"\n{name}: images {s[0]:,} and {s[1]:,} cycles one by one, {s[2]:,} as one batch of two")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("run", "table"))
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--only", nargs="*")
    args = parser.parse_args()
    if args.action == "table":
        table(); return
    os.makedirs(OUT, exist_ok=True)
    todo = [c for c in configurations() if not args.only or c["name"] in args.only]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for name, outcome in pool.map(run_one, todo):
            print(name, outcome, flush=True)


if __name__ == "__main__":
    main()
