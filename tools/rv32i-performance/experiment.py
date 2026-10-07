#!/usr/bin/env python3
"""Serial, resource-bounded RV32I calibration and experiment driver."""
import argparse
import csv
import hashlib
import json
import platform
import statistics
import subprocess
import time
from pathlib import Path
from benchmark_suite import core_gap_cases, generated_regression_cases

ROOT = Path(__file__).resolve().parents[2]
POLICIES = ("not-taken", "taken", "backward-taken", "one-bit", "two-bit")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_stamp(snapshot=None):
    paths = snapshot["source_files"] if snapshot else subprocess.check_output(
        ["git", "ls-files", "-co", "--exclude-standard", "-z"], cwd=ROOT
    ).decode().split("\0")
    digest = hashlib.sha256()
    for name in sorted(set(paths)):
        path = ROOT / name
        if snapshot or (path.is_file() and path.suffix in {".cpp", ".hpp", ".tpp", ".h", ".c", ".S", ".ld", ".py", ".sh", ".txt", ".lock", ".json", ".yml", ".cmake"}):
            digest.update(name.encode() + b"\0" + path.read_bytes())
    return digest.hexdigest()


def invoke(binary, args, timeout):
    proc = subprocess.run([str(binary), *map(str, args)], capture_output=True,
                          text=True, timeout=timeout, cwd=ROOT)
    if proc.returncode:
        raise RuntimeError(f"{' '.join(map(str, args))}: {proc.stderr[-4000:]}")
    return json.loads(proc.stdout)


def signature(row):
    return tuple(row[k] for k in ("cycles", "instruction_attempts", "retired_instructions",
                                  "terminal_attempts", "observations", "workload_fnv1a64", "final_state_fnv1a64"))


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/rv32i_experiment")
    parser.add_argument("--source-snapshot", action="store_true",
                        help="Use the frozen source-snapshot.json beside this copied tool tree")
    parser.add_argument("--output", type=Path, required=True, help="New directory; never overwrite an experiment")
    parser.add_argument("--programs", default="17,18,19,20,21,22")
    parser.add_argument("--cores", default="single,baseline,advanced")
    parser.add_argument("--policies", default=",".join(POLICIES))
    parser.add_argument("--entries", type=int, default=16)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--warmups", type=int, default=1, help="Host warmups; each CPU still starts cold")
    parser.add_argument("--timeout", type=int, default=120, help="Seconds per process")
    parser.add_argument("--reference-check", action="store_true", help="Check every selected case structurally at its modeled period")
    parser.add_argument("--clock-mode", choices=["reference", "binary-safe"], default="reference",
                        help="Per-core clock source; binary-safe requires a verified binary-startup certificate")
    parser.add_argument("--period", type=int, help="Common scheduling period instead of modeled per-core clocks")
    parser.add_argument("--allocation-limit-mib", type=int,
                        help="Address-space allocation limit for every experiment subprocess")
    parser.add_argument("--workload", help="Name from rv32i_experiment --list-workloads")
    parser.add_argument("--suite", choices=["core-gap", "generated-regression"],
                        help="Named generated-workload panel")
    parser.add_argument("--iterations", type=int, default=128)
    parser.add_argument("--size", type=int, default=16)
    parser.add_argument("--distance", type=int, default=1)
    parser.add_argument("--branch-spacing", type=int, default=16)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--pattern", default="alternating",
                        choices=["taken", "not-taken", "alternating", "bursts", "irregular"])
    parser.add_argument("--measure-body", action="store_true")
    parser.add_argument("--training-iterations", type=int, default=0)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--elf-dir", type=Path,
                        help="Run every ELF below this directory as a separate workload")
    parser.add_argument("--max-cycles", type=int, default=100000)
    parser.add_argument("--max-events", type=int, default=100000000)
    parser.add_argument("--start-pc", type=lambda s: int(s, 0))
    parser.add_argument("--stop-pc", type=lambda s: int(s, 0))
    a = parser.parse_args()
    snapshot = json.loads((ROOT / "source-snapshot.json").read_text()) if a.source_snapshot else None
    if snapshot and source_stamp(snapshot) != snapshot["source_sha256"]:
        raise RuntimeError("Frozen source snapshot was modified")
    if (a.repetitions < 1 or a.warmups < 0 or a.timeout < 1
            or (a.allocation_limit_mib is not None and a.allocation_limit_mib < 1)
            or (a.period is not None and (a.period < 4 or a.period % 2))):
        parser.error("Invalid repetition or timeout budget")
    a.binary = a.binary.resolve()
    programs = [int(x) for x in a.programs.split(",")]
    cores = a.cores.split(",")
    policies = a.policies.split(",")
    if any(c not in {"single", "baseline", "advanced"} for c in cores) or any(p not in POLICIES for p in policies):
        parser.error("Unknown core or policy")
    if sum(bool(value) for value in (a.elf, a.elf_dir, a.workload, a.suite)) > 1:
        parser.error("Choose one workload, suite, ELF or ELF directory")
    if a.suite and (a.elf or a.elf_dir or a.workload or a.start_pc is not None or a.stop_pc is not None
                    or a.measure_body or a.training_iterations or a.distance != 1
                    or a.branch_spacing != 16 or a.pattern != "alternating"):
        parser.error("Suite owns workload/region options; configure iterations, size and seed instead")
    if not (1 <= a.iterations <= 1000000 and 1 <= a.size <= 256 and 1 <= a.seed <= 0xffffffff):
        parser.error("Invalid iterations, size or seed")
    workloads = [(f"program-{n:02}", ["--program", n]) for n in programs]
    if a.suite:
        workloads = (core_gap_cases if a.suite == "core-gap" else generated_regression_cases)(
            a.iterations, a.size, a.seed)
    elif a.workload:
        if a.workload not in invoke(a.binary, ["--list-workloads"], a.timeout):
            parser.error("Unknown workload")
        args = ["--workload", a.workload, "--iterations", a.iterations]
        for key in ("size", "distance", "branch_spacing", "seed", "pattern", "training_iterations"):
            args.extend(["--" + key.replace("_", "-"), getattr(a, key)])
        if a.measure_body:
            args.append("--measure-body")
        workloads = [(a.workload, args)]
    elif a.elf:
        workloads = [(a.elf.name, ["--elf", a.elf.resolve()])]
    elif a.elf_dir:
        directory = a.elf_dir.resolve()
        paths = sorted(directory.rglob("*.elf"))
        if not paths:
            parser.error("ELF directory contains no .elf files")
        workloads = [(str(path.relative_to(directory)), ["--elf", path]) for path in paths]
    elif (a.measure_body or a.training_iterations or a.size != 16 or a.distance != 1
          or a.branch_spacing != 16 or a.seed != 1 or a.pattern != "alternating"):
        parser.error("Benchmark parameters require --workload or --suite")
    configs = [(c, p) for c in cores for p in (policies if c == "advanced" else ["not-taken"])]
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / "manifests").mkdir()
    def compact(row):
        row = dict(row)
        manifest = row.pop("manifest", None)
        if manifest is not None:
            name = sha(manifest.encode()) + ".txt"
            path = out / "manifests" / name
            if not path.exists():
                path.write_text(manifest)
            row["manifest_file"] = "manifests/" + name
        return row
    provenance = {
        "schema": 2, "result_schema": 1, "started_unix": time.time(), "command": vars(a) | {"binary": str(a.binary), "output": str(out), "elf": str(a.elf) if a.elf else None, "elf_dir": str(a.elf_dir) if a.elf_dir else None},
        "git_head": snapshot["git_head"] if snapshot else subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "git_status": snapshot["git_status"] if snapshot else subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        "source_sha256": source_stamp(snapshot), "binary_sha256": sha(a.binary.read_bytes()),
        "host": platform.uname()._asdict(), "clock_units": "normalized ticks, not physical frequency",
        "cases": [{"label": label, "arguments": list(map(str, args))} for label, args in workloads],
    }
    if a.elf:
        provenance["elf_sha256"] = sha(a.elf.read_bytes())
    if a.elf_dir:
        provenance["elf_sha256"] = {label: sha(Path(args[1]).read_bytes()) for label, args in workloads}
        build_manifest = a.elf_dir.resolve() / "builds.json"
        if build_manifest.is_file():
            provenance["elf_build_manifest_sha256"] = sha(build_manifest.read_bytes())
            provenance["elf_build_manifest"] = json.loads(build_manifest.read_text())
    (out / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    budgets = ["--max-cycles", a.max_cycles, "--max-events", a.max_events]
    if a.allocation_limit_mib is not None:
        budgets += ["--allocation-limit-mib", a.allocation_limit_mib]
    if a.start_pc is not None:
        budgets += ["--start-pc", a.start_pc]
    if a.stop_pc is not None:
        budgets += ["--stop-pc", a.stop_pc]
    summaries, calibrations, architectural_results = [], [], {}
    with (out / "runs.jsonl").open("w") as raw:
        for core, policy in configs:
            base = ["--core", core]
            if core == "advanced":
                base += ["--policy", policy, "--entries", a.entries]
            if a.clock_mode == "binary-safe":
                timing = invoke(a.binary, [*base, "--safe-clock", "--binary-startup", *budgets], a.timeout)
                safety = timing["safe_clock"]
                certified = safety["safe_period_ticks"]
                if (timing["analysis_scope"] != "binary-initialized"
                        or safety["status"] != "safe-under-contracts"
                        or not safety["binary_startup_verified"]
                        or not isinstance(certified, int) or certified < 4):
                    raise RuntimeError(f"No binary-startup safe clock for {core}/{policy}")
                period = a.period if a.period is not None else certified
                if period < certified:
                    raise RuntimeError(f"Period {period} is below certified bound {certified} for {core}/{policy}")
                calibration = {"core": core, "policy": policy, "period": period,
                               "clock_mode": a.clock_mode, "certified_period": certified,
                               "analysis_scope": timing["analysis_scope"], "safe_clock": safety,
                               "manifest_file": compact(timing)["manifest_file"], "dynamic_checks": []}
            else:
                timing = invoke(a.binary, [*base, "--timing", *budgets], a.timeout)
                period = a.period if a.period is not None else timing["timing"]["recommended_period"]
                if not timing["timing"]["valid"]:
                    raise RuntimeError(f"Invalid timing reference for {core}/{policy}")
                calibration = {"core": core, "policy": policy, "period": period, "timing": timing["timing"],
                               "manifest_file": compact(timing)["manifest_file"], "dynamic_checks": []}
            calibrations.append(calibration)
            for label, workload in workloads:
                print(f"{core}/{policy}/{label}: period={period}", flush=True)
                args = [*base, *workload, "--period", period, *budgets]
                checked = invoke(a.binary, args, a.timeout)
                assert checked["verified"]
                if (checked["benchmark_run"]["status"] != "completed"
                        or not checked["benchmark_run"]["validated"]):
                    raise RuntimeError(f"Invalid checked benchmark envelope: {core}/{policy}/{label}")
                architectural = tuple(checked[k] for k in
                    ("workload_fnv1a64", "final_state_fnv1a64", "retired_instructions", "terminal_attempts"))
                if architectural_results.setdefault(label, architectural) != architectural:
                    raise RuntimeError(f"Cross-core workload/result mismatch: {label}")
                raw.write(json.dumps(compact(checked) | {"phase": "oracle-check", "label": label}) + "\n")
                raw.flush()
                if a.reference_check:
                    reference = invoke(a.binary, [*args, "--fidelity", "reference"], a.timeout)
                    if signature(reference) != signature(checked):
                        raise RuntimeError(f"Structural/behavioral mismatch: {core}/{policy}/{label}")
                    raw.write(json.dumps(compact(reference) | {"phase": "reference", "label": label}) + "\n")
                    raw.flush()
                    calibration["dynamic_checks"].append(label)
                samples = []
                for repeat in range(-a.warmups, a.repetitions):
                    row = invoke(a.binary, [*args, "--no-verify"], a.timeout)
                    if signature(row) != signature(checked):
                        raise RuntimeError("Measurement differs from verified run")
                    row.update(phase="host-warmup" if repeat < 0 else "measurement", repetition=repeat, label=label)
                    # Explicit scheduling clocks are never represented as hardware speed.
                    if a.period is None:
                        row["clock_basis"] = ("binary-startup-safe-clock-v1" if a.clock_mode == "binary-safe"
                                              else "reference-pin-arcs-v1")
                        row["modeled_time_ticks"] = row["cycles"] * period
                    raw.write(json.dumps(compact(row)) + "\n")
                    raw.flush()
                    if repeat >= 0:
                        samples.append(row)
                times = [r["host_execution_seconds"] for r in samples]
                summary = {k: samples[0][k] for k in ("core", "policy", "entries", "cycles", "retired_instructions",
                          "cpi", "ipc", "period_ticks", "clock_basis", "modeled_time_ticks", "processed_events")}
                envelope = samples[0]["benchmark_run"]
                summary.update(workload=label, host_seconds_median=statistics.median(times),
                               host_seconds_min=min(times), host_seconds_max=max(times),
                               peak_rss_kib=max(r["peak_rss_kib"] for r in samples),
                               structural_checked=a.reference_check,
                               result_schema=envelope["schema"], run_status=envelope["status"],
                               workload_id=envelope["workload"]["id"], target_id=envelope["target"]["id"])
                for key in ("iterations", "size", "distance", "pattern", "branch_spacing", "seed",
                            "training_iterations", "window", "start_pc", "stop_pc",
                            "benchmark_result", "benchmark_expected_result"):
                    summary[key] = samples[0][key]
                # These are overlapping observations, NOT additive CPI terms.
                summary.update({"obs_" + key: value for key, value in samples[0]["observations"].items()})
                summaries.append(summary)
                (out / "clocks.json").write_text(json.dumps(calibrations, indent=2) + "\n")
    with (out / "summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summaries[0]))
        writer.writeheader();writer.writerows(summaries)
    (out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    if source_stamp(snapshot) != provenance["source_sha256"] or sha(a.binary.read_bytes()) != provenance["binary_sha256"]:
        raise RuntimeError("Source or binary changed during experiment; results are not a frozen-source campaign")
    if a.elf and sha(a.elf.read_bytes()) != provenance["elf_sha256"]:
        raise RuntimeError("ELF changed during experiment")
    if a.elf_dir:
        if {label: sha(Path(args[1]).read_bytes()) for label, args in workloads} != provenance["elf_sha256"]:
            raise RuntimeError("ELF collection changed during experiment")
        if "elf_build_manifest_sha256" in provenance and sha(build_manifest.read_bytes()) != provenance["elf_build_manifest_sha256"]:
            raise RuntimeError("ELF build manifest changed during experiment")
    (out / "COMPLETE").write_text("All selected checks and repetitions passed.\n")
    print(f"Completed {len(summaries)} cases: {out}")


if __name__ == "__main__":
    main()
