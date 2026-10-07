#!/usr/bin/env python3
"""Single serial entry point for RV32I and memory benchmark campaigns."""

import argparse
import csv
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MEMORY_BINARIES = (
    "MemoryStorageBenchmark",
    "MemoryEndpointBenchmark",
    "FFMemoryBenchmark",
    "SRAMBenchmark",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract_rv32i_records(output):
    destination = output / "benchmark-runs.jsonl"
    with (output / "runs.jsonl").open() as source, destination.open("w") as stream:
        for line in source:
            row = json.loads(line)
            record = row.get("benchmark_run")
            if record is not None:
                stream.write(json.dumps(record, separators=(",", ":")) + "\n")


def validate_middle_artifacts(directory):
    """Fail closed: the equivalence preset must never select full-size Embench."""
    spec = json.loads((ROOT / "tools/rv32i-benchmarks/embench-middle.json").read_text())
    manifest = json.loads((directory / "builds.json").read_text())
    lock = dict(line.split("=", 1) for line in
                (ROOT / "tools/rv32i-benchmarks/embench.lock").read_text().splitlines()
                if line and not line.startswith("#"))
    if (manifest.get("schema") != 1 or manifest.get("profile") != "middle"
            or manifest.get("suite") != spec or manifest.get("warmup_heat") != 0
            or manifest.get("kernel_repetitions") != 1
            or manifest.get("source_commit") != lock["EMBENCH_COMMIT"]):
        raise RuntimeError("Not the pinned reduced-work Embench middle suite")
    expected = {case["name"]: f'{case["name"]}/{case["elf"]}' for case in spec["cases"]}
    rows = manifest.get("builds", [])
    if len(rows) != len(expected) or {r.get("name") for r in rows} != set(expected):
        raise RuntimeError("Middle suite is incomplete or contains duplicate cases")
    for row in rows:
        relative = expected[row["name"]]
        if (row.get("exit") != 0 or row.get("elf") != relative
                or not (directory / relative).is_file()
                or digest(directory / relative) != row.get("elf_sha256")):
            raise RuntimeError("Missing, failed or modified middle ELF: " + row["name"])
    actual = {str(path.relative_to(directory)) for path in directory.rglob("*.elf")}
    if actual != set(expected.values()):
        raise RuntimeError("Unexpected ELF in middle suite directory")
    return directory


def run_rv32i(args, remainder):
    presets = {
        "smoke": ["--programs", "1,17,20", "--warmups", "0", "--repetitions", "1"],
        "program-regression": ["--programs", ",".join(map(str, range(1, 23)))],
        "generated-regression": ["--suite", "generated-regression"],
        "core-gap": ["--suite", "core-gap"],
    }
    preset = presets.get(args.preset, [])
    if args.preset == "compiled-middle":
        disallowed = {"--elf", "--elf-dir", "--workload", "--suite", "--programs",
                      "--start-pc", "--stop-pc", "--measure-body"}
        if any(arg.split("=", 1)[0] in disallowed for arg in remainder):
            raise RuntimeError("compiled-middle selects complete ELFs; workload/window overrides are not allowed")
        directory = validate_middle_artifacts(args.compiled_dir.resolve())
        preset = ["--elf-dir", str(directory), "--reference-check", "--clock-mode", "binary-safe",
                  "--allocation-limit-mib", "512", "--cores", "single,baseline,advanced",
                  "--policies", "two-bit", "--max-cycles", "500000", "--max-events", "20000000000",
                  "--warmups", "0", "--repetitions", "1"]
    command = [sys.executable, str(ROOT / "tools/rv32i-performance/experiment.py"),
               "--output", str(args.output), "--binary", str(args.build_dir / "rv32i_experiment"),
               "--timeout", str(args.timeout), *preset, *remainder]
    result = subprocess.run(command, cwd=ROOT)
    if result.returncode == 0:
        extract_rv32i_records(args.output.resolve())
    return result.returncode


def run_memory(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    binary_dir = args.build_dir.resolve() / "tests"
    names = args.memory_binaries.split(",")
    unknown = sorted(set(names) - set(MEMORY_BINARIES))
    if unknown:
        raise RuntimeError("Unknown memory benchmark binaries: " + ", ".join(unknown))
    binaries = [binary_dir / name for name in names]
    missing = [str(path) for path in binaries if not path.is_file()]
    if missing:
        raise RuntimeError("Missing memory benchmark binaries: " + ", ".join(missing))
    provenance = {
        "schema": 1,
        "domain": "memory",
        "started_unix": time.time(),
        "binaries": {path.name: digest(path) for path in binaries},
    }
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    records = []
    with (output / "benchmark-runs.jsonl").open("w") as raw:
        for binary in binaries:
            process = subprocess.run([str(binary)], cwd=ROOT, capture_output=True,
                                     text=True, timeout=args.timeout)
            if process.returncode:
                raise RuntimeError(f"{binary.name}: {process.stderr[-2000:]}")
            for line in process.stdout.splitlines():
                record = json.loads(line)
                if record.get("schema") != 1 or record.get("status") != "completed":
                    raise RuntimeError(f"{binary.name}: invalid benchmark result")
                records.append(record)
                raw.write(json.dumps(record, separators=(",", ":")) + "\n")
                raw.flush()
    (output / "summary.json").write_text(json.dumps(records, indent=2) + "\n")
    with (output / "summary.csv").open("w", newline="") as stream:
        fields = ("workload_id", "target_id", "status", "validated",
                  "simulated_ticks", "host_seconds", "metrics", "fingerprints")
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for record in records:
            writer.writerow({
                "workload_id": record["workload"]["id"],
                "target_id": record["target"]["id"],
                "status": record["status"],
                "validated": record["validated"],
                "simulated_ticks": record["simulatedTicks"],
                "host_seconds": record["hostSeconds"],
                "metrics": json.dumps(record["metrics"], separators=(",", ":")),
                "fingerprints": json.dumps(record["fingerprints"], separators=(",", ":")),
            })
    (output / "COMPLETE").write_text(f"{len(records)} memory benchmark results completed.\n")
    print(f"Completed {len(records)} memory benchmark results: {output}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("domain", choices=("rv32i", "memory"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--timeout", type=int,
                        help="Seconds per subprocess; default 300, or 7200 for compiled-middle")
    parser.add_argument("--preset", choices=("smoke", "program-regression",
                                               "generated-regression", "core-gap", "compiled-middle"),
                        help="RV32I workload selection; explicit trailing options may refine it")
    parser.add_argument("--memory-binaries", default=",".join(MEMORY_BINARIES),
                        help="Comma-separated memory benchmark executables")
    parser.add_argument("--compiled-dir", type=Path,
                        help="Artifact directory for --preset compiled-middle")
    args, remainder = parser.parse_known_args()
    if args.timeout is None:
        args.timeout = 7200 if args.preset == "compiled-middle" else 300
    if args.compiled_dir is not None and (args.domain != "rv32i" or args.preset != "compiled-middle"):
        parser.error("--compiled-dir requires rv32i --preset compiled-middle")
    if args.compiled_dir is None:
        args.compiled_dir = ROOT / "build/rv32i-embench-middle/artifacts"
    if args.timeout < 1:
        parser.error("Timeout must be positive")
    if remainder and remainder[0] == "--":
        remainder = remainder[1:]
    if args.domain == "memory":
        if args.preset:
            parser.error("Presets apply only to RV32I campaigns")
        if remainder:
            parser.error("Memory campaigns do not accept RV32I options")
        return run_memory(args)
    if args.memory_binaries != ",".join(MEMORY_BINARIES):
        parser.error("--memory-binaries applies only to memory campaigns")
    return run_rv32i(args, remainder)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(2)
