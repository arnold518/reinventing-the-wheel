"""Middle-suite manifest/CLI regression tests, with optional real ELF preflight.

No compiler/download or large structural run in the default test. The shared
campaign's --reference-check performs complete structural comparisons.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--artifacts", type=Path)
parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
options, remaining = parser.parse_known_args()
sys.argv = [sys.argv[0], *remaining]
spec = importlib.util.spec_from_file_location("benchmark_campaign", ROOT / "tools/benchmarks/run.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
SUITE = json.loads((ROOT / "tools/rv32i-benchmarks/embench-middle.json").read_text())
COMMIT = next(line.split("=", 1)[1] for line in
              (ROOT / "tools/rv32i-benchmarks/embench.lock").read_text().splitlines()
              if line.startswith("EMBENCH_COMMIT="))


class MiddleBenchmark(unittest.TestCase):
    def fixtures(self, directory):
        """Fake images only exercise manifest selection, never circuit execution."""
        manifest = {"schema": 1, "profile": "middle", "suite": copy.deepcopy(SUITE),
                    "source_commit": COMMIT, "kernel_repetitions": 1, "warmup_heat": 0, "builds": []}
        for case in SUITE["cases"]:
            path = directory / case["name"] / case["elf"]
            path.parent.mkdir()
            path.write_bytes(case["name"].encode())
            manifest["builds"].append({"name": case["name"], "elf": str(path.relative_to(directory)),
                                       "exit": 0, "elf_sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        self.save(directory, manifest)
        return manifest

    def save(self, directory, manifest):
        (directory / "builds.json").write_text(json.dumps(manifest))

    def test_exact_three_reduced_work_cases(self):
        self.assertEqual({case["name"] for case in SUITE["cases"]},
                         {"crc32", "nettle-sha256", "sglib-combined"})
        self.assertEqual(SUITE["kernel_repetitions"], 1)
        self.assertEqual(SUITE["warmup_heat"], 0)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            self.fixtures(path)
            self.assertEqual(runner.validate_middle_artifacts(path), path)

    def test_wrong_profile_repetitions_source_and_missing_cases_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            original = self.fixtures(path)
            for key, value in (("profile", "full"), ("warmup_heat", 1), ("kernel_repetitions", 170),
                               ("source_commit", "wrong"), ("builds", original["builds"][:-1]),
                               ("builds", [original["builds"][0]] * 3)):
                with self.subTest(key=key):
                    bad = copy.deepcopy(original)
                    bad[key] = value
                    self.save(path, bad)
                    with self.assertRaises(RuntimeError):
                        runner.validate_middle_artifacts(path)

    def test_bad_hash_failed_build_path_escape_and_extra_elf_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            original = self.fixtures(path)
            for key, value in (("elf_sha256", "wrong"), ("exit", 1), ("elf", "../outside.elf")):
                bad = copy.deepcopy(original)
                bad["builds"][0][key] = value
                self.save(path, bad)
                with self.assertRaises(RuntimeError):
                    runner.validate_middle_artifacts(path)
            self.save(path, original)
            (path / "full-size.elf").write_bytes(b"unexpected")
            with self.assertRaises(RuntimeError):
                runner.validate_middle_artifacts(path)

    def test_preset_keeps_common_driver_and_enforces_reference_check(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            self.fixtures(path)
            args = argparse.Namespace(preset="compiled-middle", compiled_dir=path,
                                      output=path / "results", build_dir=options.build_dir, timeout=7200)
            # Return failure so no real result extraction is attempted by this mock.
            with patch.object(runner.subprocess, "run", return_value=argparse.Namespace(returncode=2)) as launch:
                self.assertEqual(runner.run_rv32i(args, []), 2)
            command = launch.call_args.args[0]
            self.assertIn(str(ROOT / "tools/rv32i-performance/experiment.py"), command)
            self.assertIn("--reference-check", command)
            for flag, expected in (("--elf-dir", str(path)), ("--clock-mode", "binary-safe"),
                                   ("--allocation-limit-mib", "512"), ("--policies", "two-bit"),
                                   ("--warmups", "0"), ("--repetitions", "1")):
                self.assertEqual(command[command.index(flag) + 1], expected)
            for flag in ("--elf", "--elf-dir", "--programs", "--workload", "--suite", "--stop-pc"):
                with self.assertRaises(RuntimeError):
                    runner.run_rv32i(args, [flag + "=anything"])

    def test_abbreviated_workload_override_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / "must-not-run"
            result = subprocess.run([sys.executable, str(ROOT / "tools/rv32i-performance/experiment.py"),
                                     "--output", str(out), "--elf-d=unvalidated-full-suite"],
                                    capture_output=True, text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unrecognized arguments", result.stderr)
            self.assertFalse(out.exists())

    def test_preset_end_to_end_with_small_loader_fixture(self):
        # Exercise the real preset/driver/schema cheaply. These stand-in ELF
        # images test orchestration only, NOT equivalence of the real kernels.
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            manifest = self.fixtures(directory)
            image = (ROOT / "tests/fixtures/rv32i/external-smoke/self-check.elf").read_bytes()
            for row in manifest["builds"]:
                (directory / row["elf"]).write_bytes(image)
                row["elf_sha256"] = hashlib.sha256(image).hexdigest()
            self.save(directory, manifest)
            output = directory / "results"
            result = subprocess.run([sys.executable, str(ROOT / "tools/benchmarks/run.py"), "rv32i",
                                     "--preset", "compiled-middle", "--compiled-dir", str(directory),
                                     "--build-dir", str(options.build_dir.resolve()), "--output", str(output),
                                     "--timeout", "60", "--", "--cores", "single", "--max-cycles", "200"],
                                    capture_output=True, text=True, timeout=75)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue((output / "COMPLETE").exists())
            summaries = json.loads((output / "summary.json").read_text())
            self.assertEqual(len(summaries), 3)
            self.assertTrue(all(row["structural_checked"] for row in summaries))
            rows = [json.loads(line) for line in (output / "runs.jsonl").read_text().splitlines()]
            self.assertEqual(len(rows), 9)
            self.assertEqual({row["phase"] for row in rows}, {"oracle-check", "reference", "measurement"})
            records = [json.loads(line) for line in (output / "benchmark-runs.jsonl").read_text().splitlines()]
            self.assertEqual(len(records), 9)
            self.assertTrue(all(row["status"] == "completed" for row in records))
            provenance = json.loads((output / "provenance.json").read_text())
            self.assertEqual(provenance["elf_build_manifest"], manifest)

    @unittest.skipUnless(options.artifacts, "Supply --artifacts for the optional real compiled ELF preflight")
    def test_real_kernels_complete_in_middle_instruction_range(self):
        directory = runner.validate_middle_artifacts(options.artifacts.resolve())
        for case in SUITE["cases"]:
            with self.subTest(name=case["name"]):
                image = directory / case["name"] / case["elf"]
                proc = subprocess.run([str(options.build_dir / "rv32i_cycle_estimate"),
                                       "--elf", str(image), "--max-instructions", "200000"],
                                      text=True, capture_output=True, timeout=30)
                self.assertEqual(proc.returncode, 0, proc.stderr)
                row = json.loads(proc.stdout)
                self.assertGreater(row["instructions"], 1000)
                self.assertLessEqual(row["instructions"], 200000)
                self.assertLess(row["baseline_cycles_estimate"], 500000)


if __name__ == "__main__":
    unittest.main()
