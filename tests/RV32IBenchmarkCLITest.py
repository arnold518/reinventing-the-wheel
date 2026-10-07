"""Small structural cases on every core; larger sweeps belong to campaigns."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BINARY = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rv32i-performance"))
from benchmark_suite import core_gap_cases, generated_regression_cases


def run(*args, okay=True):
    p = subprocess.run([str(BINARY), *map(str, args), "--allocation-limit-mib", "512"],
                       capture_output=True, text=True, timeout=60)
    if okay:
        if p.returncode:
            raise AssertionError(p.stderr)
        return json.loads(p.stdout)
    if not p.returncode:
        raise AssertionError("Invalid benchmark configuration succeeded")


class BenchmarkCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.periods = {core: run("--core", core, "--safe-clock", "--binary-startup")
                       ["safe_clock"]["safe_period_ticks"] for core in ("single", "baseline", "advanced")}

    def test_all_workloads_all_cores(self):
        names = json.loads(subprocess.check_output([str(BINARY), "--list-workloads"], text=True))[3:]
        self.assertEqual(len(names), 12)
        for name in names:
            architecture = None
            for core, period in self.periods.items():
                args = ("--workload", name, "--iterations", 2, "--size", 4,
                        "--core", core, "--period", period)
                compact = run(*args)
                reference = run(*args, "--fidelity", "reference")
                with self.subTest(name=name, core=core):
                    for key in ("cycles", "observations", "retired_instructions", "terminal_attempts",
                                "workload_fnv1a64", "final_state_fnv1a64", "benchmark_result"):
                        self.assertEqual(compact[key], reference[key], key)
                    self.assertTrue(reference["verified"])
                    self.assertTrue(reference["benchmark_run"]["validated"])
                    self.assertEqual(reference["benchmark_run"]["status"], "completed")
                    self.assertEqual(compact["benchmark_result"], compact["benchmark_expected_result"])
                    signature = (compact["workload_fnv1a64"], compact["final_state_fnv1a64"])
                    if architecture is not None:
                        self.assertEqual(signature, architecture)
                    architecture = signature

    def test_equivalent_schedules(self):
        for core, period in self.periods.items():
            args = ("--core", core, "--period", period, "--iterations", 4, "--size", 8)
            a = run(*args, "--workload", "load-use")
            b = run(*args, "--workload", "load-scheduled")
            self.assertEqual(a["benchmark_result"], b["benchmark_result"])
            self.assertEqual(a["retired_instructions"], b["retired_instructions"])
            if core == "advanced":
                self.assertEqual(a["observations"]["LOAD_USE_STALL"], 32)
                self.assertEqual(b["observations"]["LOAD_USE_STALL"], 0)
                self.assertEqual(a["cycles"] - b["cycles"], 32)

    def test_body_window_and_same_pc_training(self):
        for core, period in self.periods.items():
            for name in ("independent", "branch-pattern"):
                args = ("--core", core, "--period", period, "--workload", name)
                whole = run(*args, "--iterations", 7)
                region = run(*args, "--iterations", 3, "--measure-body", "--training-iterations", 4)
                reference = run(*args, "--iterations", 3, "--measure-body", "--training-iterations", 4,
                                "--fidelity", "reference")
                self.assertEqual(whole["workload_fnv1a64"], region["workload_fnv1a64"])
                self.assertEqual(whole["final_state_fnv1a64"], region["final_state_fnv1a64"])
                self.assertEqual(whole["cycles"], region["whole_program_cycles"])
                self.assertEqual(region["cycles"], reference["cycles"])
                self.assertEqual(region["observations"], reference["observations"])
                self.assertEqual(region["terminal_attempts"], 0)
                self.assertLess(region["cycles"], whole["cycles"])
                if name == "independent":
                    self.assertEqual(region["retired_instructions"], 3 * 66)

    def test_invalid_options(self):
        for args in (("--size", 0), ("--size", 257), ("--distance", 3), ("--seed", 0),
                     ("--seed", 0x100000000), ("--training-iterations", 1),
                     ("--measure-body", "--start-pc", 0), ("--branch-spacing", 3)):
            run("--workload", "dependency", *args, okay=False)
        run("--workload", "dependent", "--measure-body", okay=False)

    def test_catalog_and_bounded_calibration(self):
        catalog = json.loads(subprocess.check_output(
            [str(BINARY), "--list-benchmark-catalog"], text=True))
        self.assertEqual(len(catalog), 37)
        self.assertEqual(len({row["id"] for row in catalog}), 37)
        result = run("--core", "single", "--workload", "dependent", "--iterations", 100,
                     "--period", self.periods["single"], "--max-cycles", 10,
                     "--allow-incomplete")
        self.assertFalse(result["completed"])
        self.assertTrue(result["prefix_verified"])
        self.assertFalse(result["verified"])
        self.assertEqual(result["benchmark_run"]["status"], "incomplete")
        self.assertFalse(result["benchmark_run"]["validated"])

    def test_campaign_driver(self):
        cases = core_gap_cases(4, 8, 1)
        self.assertEqual(len(cases), 23)
        self.assertEqual(len(set(label for label, _ in cases)), len(cases))
        with tempfile.TemporaryDirectory(prefix="ritw-benchmark-") as temp:
            out = Path(temp) / "campaign"
            subprocess.run([sys.executable, str(ROOT / "tools/rv32i-performance/experiment.py"),
                            "--binary", str(BINARY), "--output", str(out), "--cores", "single,advanced",
                            "--policies", "two-bit", "--workload", "scan", "--size", "4", "--iterations", "2",
                            "--clock-mode", "binary-safe", "--reference-check", "--allocation-limit-mib", "512",
                            "--warmups", "0", "--repetitions", "1"], check=True, capture_output=True, timeout=60)
            self.assertTrue((out / "COMPLETE").exists())
            rows = json.loads((out / "summary.json").read_text())
            self.assertEqual(len(rows), 2)
            for row in rows:
                self.assertTrue(row["structural_checked"])
                self.assertEqual(row["modeled_time_ticks"], row["cycles"] * row["period_ticks"])
                self.assertEqual(row["benchmark_result"], row["benchmark_expected_result"])
                self.assertIn("obs_LOAD_USE_STALL", row)

        generated = generated_regression_cases(4, 8, 1)
        self.assertEqual(len(generated), 15)
        self.assertEqual(len({label for label, _ in generated}), 15)


if __name__ == "__main__":
    unittest.main()
