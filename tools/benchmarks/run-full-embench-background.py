#!/usr/bin/env python3
"""Detached, serial Embench campaign; one failure never skips later jobs."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
CONFIGS = [("single", "not-taken"), ("baseline", "not-taken")] + [
    ("advanced", policy) for policy in
    ("not-taken", "taken", "backward-taken", "one-bit", "two-bit")]
MIDDLE_CONFIGS = [("single", "not-taken"), ("baseline", "not-taken"), ("advanced", "two-bit")]
SOURCE_SUFFIXES = {".cpp", ".hpp", ".tpp", ".h", ".c", ".S", ".ld", ".py", ".sh",
                   ".txt", ".lock", ".json", ".yml", ".cmake"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def memory_cap():
    resource.setrlimit(resource.RLIMIT_AS, (512 * 1024**2, 512 * 1024**2))


def budgets(estimate, core, reference_check=False):
    count = estimate["instructions"]
    baseline = estimate["baseline_cycles_estimate"]
    expected = count if core == "single" else baseline if core == "baseline" else (
        count * 1.25 + 2 * estimate["control_flow_instructions"] + 8)
    # Deliberately loose budgets, not timing predictions. Zero-wait memory only.
    bound = count if core == "single" else baseline if core == "baseline" else 4 * count
    cycles = math.ceil(bound * 1.1) + 128
    seconds = expected * {"single": 0.0005, "baseline": 0.00025, "advanced": 0.00012}[core]
    if reference_check:
        # Same safety caps as the compiled-middle preset, not behavioral event budgets.
        # Pilot-derived costs are rough planning estimates, not measured results.
        reference_seconds = expected * {"single": 0.041, "baseline": 0.0174, "advanced": 0.033}[core]
        return {"max_cycles": 500000, "max_events": 20000000000,
                "timeout_seconds": 7200, "estimated_seconds_per_execution": round(seconds, 2),
                "estimated_job_seconds": round(2 * seconds + reference_seconds, 2)}
    return {"max_cycles": cycles, "max_events": max(100000000, cycles * 128),
            "timeout_seconds": max(14400, math.ceil(seconds * 4 + 900)),
            "estimated_seconds_per_execution": round(seconds, 2),
            "estimated_job_seconds": round(2 * seconds, 2)}


def freeze(output, artifacts, expected_workloads, build_dir=ROOT / "build", profile="full"):
    if profile not in ("full", "middle"):
        raise ValueError("Unknown Embench profile")
    reference_check = profile == "middle"
    configs = MIDDLE_CONFIGS if reference_check else CONFIGS
    if reference_check:
        spec = importlib.util.spec_from_file_location("benchmark_runner", ROOT / "tools/benchmarks/run.py")
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        runner.validate_middle_artifacts(artifacts)
    elfs = sorted(artifacts.rglob("*.elf"))
    names = [elf.stem for elf in elfs]
    if len(elfs) != expected_workloads or len(set(names)) != len(names):
        raise RuntimeError(f"Expected {expected_workloads} uniquely named ELFs, found {names}")
    for name in names:
        if not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name):
            raise RuntimeError("Unsafe workload name")
    for name in ("rv32i_experiment", "rv32i_cycle_estimate"):
        if not (build_dir / name).is_file():
            raise RuntimeError(f"Missing build/{name}")
    output.mkdir(parents=True, exist_ok=False)
    snapshot = output / "snapshot"
    snapshot.mkdir()
    paths = subprocess.check_output(["git", "ls-files", "-co", "--exclude-standard", "-z"],
                                    cwd=ROOT).decode().split("\0")
    sources = sorted({name for name in paths if (ROOT / name).is_file()
                      and Path(name).suffix in SOURCE_SUFFIXES})
    source_hash = hashlib.sha256()
    for name in sources:
        src, dst = ROOT / name, snapshot / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        source_hash.update(name.encode() + b"\0" + dst.read_bytes())
        dst.chmod(0o444)
    (snapshot / "build").mkdir(exist_ok=True)
    binaries = {}
    for name in ("rv32i_experiment", "rv32i_cycle_estimate"):
        dst = snapshot / "build" / name
        shutil.copy2(build_dir / name, dst)
        dst.chmod(0o555)
        binaries[name] = digest(dst)
    workloads = []
    (snapshot / "elf").mkdir()
    for elf in elfs:
        dst = snapshot / "elf" / elf.name
        shutil.copy2(elf, dst)
        dst.chmod(0o444)
        workloads.append({"name": elf.stem, "elf": str(dst.relative_to(snapshot)), "sha256": digest(dst)})
    if (artifacts / "builds.json").exists():
        shutil.copy2(artifacts / "builds.json", output / "builds.json")
    provenance = {"schema": 1, "created_unix": time.time(),
                  "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  "git_status": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                  "source_files": sources, "source_sha256": source_hash.hexdigest(),
                  "binary_sha256": binaries}
    save(snapshot / "source-snapshot.json", provenance)
    manifest = {"schema": 1, "created_unix": time.time(), "status": "prepared", "profile": profile,
                "fidelity": "behavioral", "clock_mode": "binary-safe", "allocation_limit_mib": 512,
                "reference_check": reference_check,
                "verification_runs": 2 if reference_check else 1, "measurement_runs": 1, "warmups": 0,
                "workloads": workloads, "jobs": [
                    {"id": f"{workload['name']}/{core}-{policy}", "workload": workload["name"],
                     "core": core, "policy": policy, "reference_check": reference_check, "status": "pending"}
                    for workload in workloads for core, policy in configs]}
    save(output / "campaign.json", manifest)
    return snapshot


def check_snapshot(snapshot):
    provenance = json.loads((snapshot / "source-snapshot.json").read_text())
    for name, expected in provenance["binary_sha256"].items():
        if digest(snapshot / "build" / name) != expected:
            raise RuntimeError("Frozen binary modified: " + name)


def architectural_signature(row):
    return tuple(row[key] for key in ("workload_fnv1a64", "final_state_fnv1a64",
                                      "retired_instructions", "terminal_attempts"))


def run_job(output, snapshot, job, workload):
    destination = output / "cases" / job["id"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    if digest(snapshot / workload["elf"]) != workload["sha256"]:
        raise RuntimeError("Frozen ELF modified")
    check_snapshot(snapshot)
    command = [sys.executable, "-u", str(snapshot / "tools/benchmarks/run.py"), "rv32i",
               "--output", str(destination), "--timeout", str(job["timeout_seconds"]), "--",
               "--source-snapshot", "--elf", str(snapshot / workload["elf"]),
               "--cores", job["core"], "--policies", job["policy"],
               "--clock-mode", "binary-safe", "--allocation-limit-mib", "512",
               "--max-cycles", str(job["max_cycles"]), "--max-events", str(job["max_events"]),
               "--warmups", "0", "--repetitions", "1"]
    if job.get("reference_check"):
        command.append("--reference-check")
    with destination.with_suffix(".log").open("w") as log:
        # The inner driver bounds each native subprocess; allow calibration and both runs.
        result = subprocess.run(command, cwd=snapshot, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode or not (destination / "COMPLETE").exists():
        raise RuntimeError(f"Campaign exit {result.returncode}; see {destination}.log")
    rows = [json.loads(line) for line in (destination / "runs.jsonl").read_text().splitlines()]
    checked = next(row for row in rows if row["phase"] == "oracle-check")
    if checked["retired_instructions"] != workload["preflight"]["instructions"]:
        raise RuntimeError("Circuit retirement count differs from preflight oracle")
    if job.get("reference_check"):
        reference = next(row for row in rows if row["phase"] == "reference")
        if reference["fidelity"] != "reference" or architectural_signature(reference) != architectural_signature(checked):
            raise RuntimeError("Missing or mismatched structural-reference execution")
    return architectural_signature(checked)


def worker(output):
    snapshot = output / "snapshot"
    manifest = json.loads((output / "campaign.json").read_text())
    manifest.update(status="preflight", pid=os.getpid(), started_unix=time.time())
    save(output / "campaign.json", manifest)
    os.nice(10)
    check_snapshot(snapshot)
    for workload in manifest["workloads"]:
        try:
            result = subprocess.run([str(snapshot / "build/rv32i_cycle_estimate"), "--elf",
                                     str(snapshot / workload["elf"])], cwd=snapshot,
                                    capture_output=True, text=True, timeout=1800, preexec_fn=memory_cap)
            if result.returncode:
                raise RuntimeError(result.stderr.strip())
            workload["preflight"] = json.loads(result.stdout)
            print(f"Preflight {workload['name']}: {workload['preflight']['instructions']} instructions", flush=True)
        except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
            workload["preflight_error"] = str(error)
            print(f"Preflight FAILED {workload['name']}: {error}", flush=True)
        save(output / "campaign.json", manifest)
    lookup = {workload["name"]: workload for workload in manifest["workloads"]}
    for job in manifest["jobs"]:
        workload = lookup[job["workload"]]
        if "preflight" in workload:
            job.update(budgets(workload["preflight"], job["core"], job.get("reference_check", False)))
        else:
            job.update(status="blocked", error=workload["preflight_error"])
    # Shortest workloads first; all configurations of each workload remain together.
    manifest["jobs"].sort(key=lambda job: lookup[job["workload"]].get("preflight", {}).get("instructions", float("inf")))
    manifest["estimated_host_seconds"] = sum(job.get("estimated_job_seconds", 0)
                                              for job in manifest["jobs"])
    manifest["status"] = "running"
    save(output / "campaign.json", manifest)
    signatures = {}
    for job in manifest["jobs"]:
        if job["status"] != "pending":
            continue
        job.update(status="running", started_unix=time.time())
        manifest["current_job"] = job["id"]
        save(output / "campaign.json", manifest)
        print(f"Starting {job['id']}: cycle cap {job['max_cycles']}, timeout {job['timeout_seconds']}s/process", flush=True)
        try:
            signature = run_job(output, snapshot, job, lookup[job["workload"]])
            if signatures.setdefault(job["workload"], signature) != signature:
                raise RuntimeError("Cross-core/predictor architectural result mismatch")
            job.update(status="passed", architectural_signature=signature)
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyError, StopIteration) as error:
            job.update(status="failed", error=str(error))
        job["finished_unix"] = time.time()
        print(f"Finished {job['id']}: {job['status']}", flush=True)
        save(output / "campaign.json", manifest)
    success = all(job["status"] == "passed" for job in manifest["jobs"])
    manifest.update(status="completed" if success else "finished-with-failures", finished_unix=time.time(), current_job=None)
    save(output / "campaign.json", manifest)
    (output / "FINISHED").write_text(manifest["status"] + "\n")
    if success:
        (output / "COMPLETE").write_text(f"All {len(manifest['jobs'])} cases passed, including cross-configuration checks.\n")
    return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("command", choices=("start", "status", "_worker"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=("full", "middle"), default="full")
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--expected-workloads", type=int, help="Override only for smoke tests")
    args = parser.parse_args()
    output = args.output.resolve()
    if args.command == "status":
        manifest = json.loads((output / "campaign.json").read_text())
        counts = {state: sum(job["status"] == state for job in manifest["jobs"])
                  for state in ("pending", "running", "passed", "failed", "blocked")}
        pid = manifest.get("pid")
        alive = False
        if pid:
            try:
                os.kill(pid, 0)
                alive = True
            except ProcessLookupError:
                pass
        print(json.dumps({"status": manifest["status"], "pid": pid, "process_alive": alive,
                          "worker_exit": (output / "worker.exit").read_text().strip()
                          if (output / "worker.exit").exists() else None,
                          "current_job": manifest.get("current_job"), "counts": counts,
                          "estimated_total_host_days": manifest.get("estimated_host_seconds", 0) / 86400}, indent=2))
        return 0
    if args.command == "_worker":
        code = 2
        try:
            code = worker(output)
            return code
        except BaseException as error:
            manifest = json.loads((output / "campaign.json").read_text())
            manifest.update(status="aborted", error=repr(error), finished_unix=time.time())
            save(output / "campaign.json", manifest)
            raise
        finally:
            (output / "worker.exit").write_text(str(code) + "\n")
    artifacts = args.artifacts or ROOT / ("build/rv32i-embench-middle/artifacts" if args.profile == "middle"
                                         else "build/rv32i-embench-multilib/artifacts")
    expected_workloads = args.expected_workloads if args.expected_workloads is not None else (
        3 if args.profile == "middle" else 19)
    snapshot = freeze(output, artifacts.resolve(), expected_workloads, args.build_dir.resolve(), args.profile)
    with (output / "campaign.log").open("w") as log:
        process = subprocess.Popen([sys.executable, "-u", str(snapshot / "tools/benchmarks" / Path(__file__).name),
                                    "_worker", "--output", str(output)], cwd=snapshot,
                                   stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
    (output / "worker.pid").write_text(str(process.pid) + "\n")
    jobs = json.loads((output / "campaign.json").read_text())["jobs"]
    print(f"Started PID {process.pid}; {len(jobs)} cases; {output}/campaign.log")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
