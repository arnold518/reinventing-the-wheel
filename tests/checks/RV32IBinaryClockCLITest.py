"""Binary-startup certificates and structural execution at their returned clocks."""
import json
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)


def invoke(*args, status=0):
    result = subprocess.run([BINARY, *map(str, args), "--allocation-limit-mib", "512"],
                            capture_output=True, text=True, timeout=60)
    if result.returncode != status:
        raise AssertionError(f"{args}: {result.returncode}\n{result.stderr}\n{result.stdout}")
    return json.loads(result.stdout) if status != 1 else None


class BinaryClockCLI(unittest.TestCase):
    def test_certificates_and_all_structural_programs(self):
        for core in ("single", "baseline", "advanced"):
            report = invoke("--core", core, "--safe-clock", "--binary-startup")
            self.assertEqual(report["analysis_scope"], "binary-initialized")
            safe = report["safe_clock"]
            self.assertEqual(safe["status"], "safe-under-contracts")
            self.assertTrue(safe["binary_startup_verified"])
            self.assertGreater(safe["binary_capture_bits"], 0)
            self.assertEqual(safe["checked_state_cases"], safe["state_case_count"])
            self.assertTrue(any("sparse default fill" in e for e in safe["binary_evidence"]))
            self.assertEqual(safe["diagnostics"], [])
            period = safe["safe_period_ticks"]
            # A regression ceiling, not a hard-coded desired calculator answer.
            self.assertLess(period, {"single": 176, "baseline": 112, "advanced": 212}[core])
            self.assertEqual(safe["state_case_count"], 4 if core == "advanced" else 1)
            self.assertGreaterEqual(period, 4)
            # No workload-specific memory/instruction values enter the proof.
            other = invoke("--core", core, "--safe-clock", "--binary-startup", "--program", 22)
            self.assertEqual(other["safe_clock"]["safe_period_ticks"], period)
            for program in range(1, 23):
                with self.subTest(core=core, program=program, period=period):
                    run = invoke("--core", core, "--fidelity", "reference", "--program", program,
                                 "--period", period)
                    self.assertTrue(run["verified"])

    def test_prediction_policies(self):
        for policy in ("not-taken", "taken", "backward-taken", "one-bit", "two-bit"):
            safe = invoke("--safe-clock", "--binary-startup", "--policy", policy)["safe_clock"]
            self.assertTrue(safe["binary_startup_verified"])
            result = invoke("--fidelity", "reference", "--policy", policy, "--program", 20,
                            "--period", safe["safe_period_ticks"])
            self.assertTrue(result["verified"])

    def test_scope_is_explicit_and_fail_closed(self):
        for core in ("single", "baseline"):
            report = invoke("--core", core, "--safe-clock")
            self.assertEqual(report["analysis_scope"], "unrestricted-four-state")
            self.assertFalse(report["safe_clock"]["binary_startup_verified"])
            self.assertEqual(report["safe_clock"]["status"], "safe-under-contracts")
            self.assertGreater(report["safe_clock"]["safe_period_ticks"], 0)
        unsupported = invoke("--safe-clock", "--binary-startup", "--memory", "sram", status=2)
        self.assertFalse(unsupported["safe_clock"]["binary_startup_verified"])
        self.assertIsNone(unsupported["safe_clock"]["safe_period_ticks"])
        invoke("--binary-startup", status=1)
        # The compact system is analyzed from its delay tables. For the advanced
        # core the ready signals of core and memory form a loop that only the
        # gates resolve, so no clock is claimed.
        compact = invoke("--safe-clock", "--binary-startup", "--fidelity", "behavioral", status=2)
        self.assertIsNone(compact["safe_clock"]["safe_period_ticks"])


if __name__ == "__main__":
    unittest.main()
