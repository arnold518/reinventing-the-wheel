"""Smoke the common campaign frontend for both current benchmark domains."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(sys.argv.pop(1)).resolve()
RUNNER = ROOT / "tools/benchmarks/run.py"


class BenchmarkCampaignCLI(unittest.TestCase):
    def records(self, output):
        self.assertTrue((output / "COMPLETE").exists())
        self.assertTrue((output / "summary.csv").exists())
        rows = [json.loads(line) for line in (output / "benchmark-runs.jsonl").read_text().splitlines()]
        self.assertTrue(rows)
        self.assertTrue(all(row["schema"] == 1 and row["status"] == "completed" for row in rows))
        return rows

    def test_rv32i_and_memory_frontends(self):
        with tempfile.TemporaryDirectory(prefix="ritw-common-benchmark-") as temporary:
            root = Path(temporary)
            rv32i = root / "rv32i"
            subprocess.run([sys.executable, str(RUNNER), "rv32i", "--output", str(rv32i),
                            "--build-dir", str(BUILD), "--preset", "smoke", "--timeout", "60", "--",
                            "--cores", "single", "--programs", "1", "--clock-mode", "binary-safe",
                            "--allocation-limit-mib", "512"], cwd=ROOT, check=True, timeout=75,
                           capture_output=True, text=True)
            self.assertTrue(all(row["workload"]["domain"] == "rv32i" for row in self.records(rv32i)))
            external = root / "external"
            subprocess.run([sys.executable, str(RUNNER), "rv32i", "--output", str(external),
                            "--build-dir", str(BUILD), "--timeout", "60", "--",
                            "--cores", "single", "--elf-dir", str(ROOT / "tests/fixtures/rv32i"),
                            "--clock-mode", "binary-safe", "--warmups", "0", "--repetitions", "1",
                            "--allocation-limit-mib", "512"], cwd=ROOT, check=True, timeout=75,
                           capture_output=True, text=True)
            external_rows = self.records(external)
            self.assertTrue(all("external" in row["workload"]["tags"] for row in external_rows))
            memory = root / "memory"
            subprocess.run([sys.executable, str(RUNNER), "memory", "--output", str(memory),
                            "--build-dir", str(BUILD), "--memory-binaries", "MemoryStorageBenchmark"],
                           cwd=ROOT, check=True, timeout=60, capture_output=True, text=True)
            self.assertTrue(all(row["workload"]["domain"] == "memory" for row in self.records(memory)))


if __name__ == "__main__":
    unittest.main()
