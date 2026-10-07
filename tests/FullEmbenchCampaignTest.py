"""Focused orchestration checks; never execute a large Embench workload."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else ROOT / "build"
SCRIPT = ROOT / "tools/benchmarks/run-full-embench-background.py"
spec = importlib.util.spec_from_file_location("full_embench", SCRIPT)
campaign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(campaign)


class FullEmbenchCampaignTest(unittest.TestCase):
    def test_estimator_matches_baseline_circuit(self):
        for program in (1, 2, 17, 20, 22):
            estimated = json.loads(subprocess.check_output(
                [str(BUILD / "rv32i_cycle_estimate"), "--program", str(program)], text=True, timeout=10))
            actual = json.loads(subprocess.check_output(
                [str(BUILD / "rv32i_experiment"), "--program", str(program), "--core", "baseline",
                 "--period", "62", "--allocation-limit-mib", "512"], text=True, timeout=30))
            self.assertEqual(estimated["instructions"], actual["retired_instructions"] + actual["terminal_attempts"])
            self.assertEqual(estimated["baseline_cycles_estimate"], actual["cycles"])

    def test_matrix_and_scaled_budgets(self):
        self.assertEqual(len(campaign.CONFIGS), 7)
        self.assertEqual(len(set(campaign.CONFIGS)), 7)
        self.assertEqual(19 * len(campaign.CONFIGS), 133)
        estimate = {"instructions": 75000000, "baseline_cycles_estimate": 191000000,
                    "control_flow_instructions": 26000000}
        for core in ("single", "baseline", "advanced"):
            row = campaign.budgets(estimate, core)
            self.assertGreater(row["max_cycles"], estimate["instructions"])
            self.assertGreater(row["timeout_seconds"], 14400)
            self.assertGreater(row["max_events"], 2000000000)

    def test_middle_structural_budgets(self):
        estimate = {"instructions": 105470, "baseline_cycles_estimate": 246502,
                    "control_flow_instructions": 20000}
        self.assertEqual(len(campaign.MIDDLE_CONFIGS), 3)
        for core, _ in campaign.MIDDLE_CONFIGS:
            row = campaign.budgets(estimate, core, reference_check=True)
            self.assertEqual(row["max_cycles"], 500000)
            self.assertEqual(row["max_events"], 20000000000)
            self.assertEqual(row["timeout_seconds"], 7200)
            self.assertGreater(row["estimated_job_seconds"], 2 * row["estimated_seconds_per_execution"])

    def freeze_fixture(self, root):
        artifacts = root / "artifacts"
        artifacts.mkdir()
        shutil.copy2(ROOT / "tests/fixtures/rv32i/external-smoke/self-check.elf", artifacts / "smoke.elf")
        output = root / "campaign"
        snapshot = campaign.freeze(output, artifacts, 1, BUILD)
        return output, snapshot

    def test_frozen_end_to_end_all_configurations(self):
        with tempfile.TemporaryDirectory(prefix="ritw-full-embench-test-") as temporary:
            output, snapshot = self.freeze_fixture(Path(temporary))
            process = subprocess.run([sys.executable, str(snapshot / SCRIPT.relative_to(ROOT)),
                                      "_worker", "--output", str(output)], capture_output=True,
                                     text=True, timeout=150)
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            self.assertTrue((output / "COMPLETE").exists())
            manifest = json.loads((output / "campaign.json").read_text())
            self.assertEqual(len(manifest["jobs"]), 7)
            self.assertTrue(all(job["status"] == "passed" for job in manifest["jobs"]))
            self.assertEqual(manifest["workloads"][0]["preflight"]["instructions"], 19)
            # Changing copied source is rejected, even with no .git in the snapshot.
            copied = snapshot / "tools/rv32i-performance/benchmark_suite.py"
            copied.chmod(0o644)
            copied.write_text(copied.read_text() + "\n# test mutation\n")
            rejected = subprocess.run([sys.executable, str(snapshot / "tools/rv32i-performance/experiment.py"),
                                       "--source-snapshot", "--output", str(output / "tampered")],
                                      capture_output=True, text=True, timeout=10)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("snapshot was modified", rejected.stderr)

    def test_middle_frozen_structural_matrix(self):
        with tempfile.TemporaryDirectory(prefix="ritw-middle-background-test-") as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            suite = json.loads((ROOT / "tools/rv32i-benchmarks/embench-middle.json").read_text())
            lock = dict(line.split("=", 1) for line in
                        (ROOT / "tools/rv32i-benchmarks/embench.lock").read_text().splitlines()
                        if line and not line.startswith("#"))
            builds = []
            # Tiny ELF fixtures exercise orchestration, never the actual large kernels.
            for case in suite["cases"]:
                elf = artifacts / case["name"] / case["elf"]
                elf.parent.mkdir(parents=True)
                shutil.copy2(ROOT / "tests/fixtures/rv32i/external-smoke/self-check.elf", elf)
                builds.append({"name": case["name"], "exit": 0,
                               "elf": str(elf.relative_to(artifacts)), "elf_sha256": campaign.digest(elf)})
            manifest = {"schema": 1, "profile": "middle", "suite": suite, "warmup_heat": 0,
                        "kernel_repetitions": 1, "source_commit": lock["EMBENCH_COMMIT"], "builds": builds}
            campaign.save(artifacts / "builds.json", manifest)
            output = root / "campaign"
            snapshot = campaign.freeze(output, artifacts, 3, BUILD, profile="middle")
            process = subprocess.run([sys.executable, str(snapshot / SCRIPT.relative_to(ROOT)),
                                      "_worker", "--output", str(output)], capture_output=True,
                                     text=True, timeout=150)
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            self.assertTrue((output / "COMPLETE").exists())
            manifest = json.loads((output / "campaign.json").read_text())
            self.assertEqual(len(manifest["jobs"]), 9)
            self.assertTrue(manifest["reference_check"])
            self.assertEqual(manifest["verification_runs"], 2)
            for job in manifest["jobs"]:
                self.assertEqual(job["status"], "passed")
                rows = [json.loads(line) for line in
                        (output / "cases" / job["id"] / "runs.jsonl").read_text().splitlines()]
                self.assertEqual([row["phase"] for row in rows], ["oracle-check", "reference", "measurement"])
                self.assertEqual(rows[1]["fidelity"], "reference")
                self.assertEqual(rows[0]["cycles"], rows[1]["cycles"])
            # The middle profile must not silently accept a full-sized build manifest.
            manifest = json.loads((artifacts / "builds.json").read_text())
            manifest["profile"] = "full"
            campaign.save(artifacts / "builds.json", manifest)
            with self.assertRaisesRegex(RuntimeError, "reduced-work"):
                campaign.freeze(root / "rejected", artifacts, 3, BUILD, profile="middle")

    def test_failure_continues_and_never_claims_complete(self):
        with tempfile.TemporaryDirectory(prefix="ritw-full-embench-failure-") as temporary:
            output, _ = self.freeze_fixture(Path(temporary))
            results = [RuntimeError("deliberate failed case")] + [("image", "state", 19, 0)] * 6
            with patch.object(campaign, "run_job", side_effect=results) as execute, patch.object(campaign.os, "nice"):
                self.assertEqual(campaign.worker(output), 1)
            self.assertEqual(execute.call_count, 7)
            manifest = json.loads((output / "campaign.json").read_text())
            self.assertEqual([job["status"] for job in manifest["jobs"]], ["failed"] + ["passed"] * 6)
            self.assertTrue((output / "FINISHED").exists())
            self.assertFalse((output / "COMPLETE").exists())


if __name__ == "__main__":
    unittest.main()
