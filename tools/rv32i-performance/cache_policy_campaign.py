#!/usr/bin/env python3
"""Cache behavior campaign (general cache plan, step 7).

Runs the advanced core over the cached memory arrangements with four cache
behaviors (write-through, write buffer, write-back, write-back with four
ways), the four campaign programs and both realistic timing profiles, one run
at a time. Each result is appended to a JSON-lines file as it finishes, so an
interrupted campaign continues where it stopped.

    tools/rv32i-performance/cache_policy_campaign.py OUT.jsonl [--only PROFILE] [--clock]

--clock runs the clock analysis of each behavior per arrangement and profile
(compact controllers) instead of the programs.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "build-cleanup" / "rv32i_experiment"
PERIODS = {"embedded": 4104, "desktop": 2858}
ARRANGEMENTS = ["C-ID", "D-0", "D-ID"]
PROGRAMS = {"calls": (16, 256), "sort": (64, 4), "stream": (1024, 4), "pointer-chase": (512, 4)}
WRITE_BACK = "write_policy=back,write_miss=allocate"
VARIANTS = {
    "WT": [],
    "BUF": ["--dl1", "write_buffer=2", "--l2", "write_buffer=2"],
    "WB": ["--dl1", WRITE_BACK, "--l2", WRITE_BACK],
    "WB4": ["--il1", "sets=4,ways=4,replacement=plru",
            "--dl1", WRITE_BACK + ",sets=4,ways=4,replacement=plru",
            "--l2", WRITE_BACK + ",sets=32,ways=4,replacement=plru"],
}


def command(profile, arrangement, variant, extra):
    return [str(TOOL), "--core", "advanced", "--memory", "dram", "--topology", arrangement,
            "--period", str(PERIODS[profile]), "--cache-controller", "behavioral",
            "--max-cycles", "20000000", "--max-events", "40000000000", *VARIANTS[variant], *extra]


def run(args, profile):
    env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE=profile)
    started = time.time()
    done = subprocess.run(args, capture_output=True, text=True, env=env)
    seconds = time.time() - started
    if done.returncode != 0:
        return {"error": (done.stderr or done.stdout).strip().splitlines()[-1:] or ["failed"], "host": seconds}
    return {"result": json.loads(done.stdout), "host": seconds}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out")
    parser.add_argument("--only")
    parser.add_argument("--clock", action="store_true")
    options = parser.parse_args()
    out = Path(options.out)
    seen = set()
    if out.exists():
        for line in out.read_text().splitlines():
            entry = json.loads(line)
            seen.add(tuple(entry["key"]))
    jobs = []
    for profile in PERIODS:
        if options.only and profile != options.only:
            continue
        for variant in VARIANTS:
            for arrangement in ARRANGEMENTS:
                if options.clock:
                    jobs.append((profile, variant, arrangement, "clock", ["--program", "20", "--safe-clock"]))
                    continue
                for program, (size, iterations) in PROGRAMS.items():
                    jobs.append((profile, variant, arrangement, program,
                                 ["--workload", program, "--size", str(size), "--iterations", str(iterations),
                                  "--measure-body", "--training-iterations", "1"]))
    for profile, variant, arrangement, program, extra in jobs:
        key = [profile, variant, arrangement, program]
        if tuple(key) in seen:
            continue
        outcome = run(command(profile, arrangement, variant, extra), profile)
        entry = {"key": key}
        if "error" in outcome:
            entry["error"] = outcome["error"]
        else:
            r = outcome["result"]
            if options.clock:
                clock = r.get("safe_clock", {})
                entry.update(status=clock.get("status"), period=clock.get("safe_period_ticks"),
                             limit=clock.get("limiting_constraint"))
            else:
                entry.update({name: r.get(name) for name in (
                    "cycles", "whole_program_cycles", "cpi", "verified", "setup_violations",
                    "retired_instructions", "final_state_fnv1a64")})
        entry["host_seconds"] = round(outcome["host"], 1)
        with out.open("a") as handle:
            handle.write(json.dumps(entry) + "\n")
        print(json.dumps(entry), flush=True)


if __name__ == "__main__":
    sys.exit(main())
