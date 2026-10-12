import json
import subprocess
import sys
import unittest
from pathlib import Path

BINARY = sys.argv.pop(1)


def run(*args, okay=True):
    p = subprocess.run([BINARY, *map(str, args), "--allocation-limit-mib", "512"], capture_output=True, text=True, timeout=45)
    if okay:
        if p.returncode:
            raise AssertionError(p.stderr)
        return json.loads(p.stdout)
    if not p.returncode:
        raise AssertionError("Invalid request succeeded")


class ExperimentCLI(unittest.TestCase):
    def test_safe_clock_is_fail_closed(self):
        for core in ("single", "baseline", "advanced"):
            p = subprocess.run([BINARY, "--core", core, "--safe-clock", "--allocation-limit-mib", "512"],
                               capture_output=True, text=True, timeout=45)
            self.assertEqual(p.returncode, 0, p.stderr)
            report = json.loads(p.stdout)
            self.assertEqual(report["clock_basis"], "safe-clock-contracts-v1")
            safety = report["safe_clock"]
            self.assertFalse(any("no all-state timing contract" in d for d in safety["diagnostics"]))
            # The memory endpoint is timed by its delay table: an estimate for
            # its real size, from the two largest sizes proven on gates.
            self.assertTrue(any("memory.ff.scalable-endpoint.32" in c and "ESTIMATE for 65536 words" in c
                                for c in safety["contracts"]))
            cycle = safety["dependency_cycle"]
            if core == "advanced":
                self.assertEqual(safety["status"], "safe-under-contracts")
                self.assertLess(safety["safe_period_ticks"], 212)
                self.assertEqual(safety["state_case_count"], 36)
                self.assertEqual(safety["checked_state_cases"], 36)
                self.assertEqual(safety["diagnostics"], [])
                self.assertEqual(cycle, [])
            else:
                self.assertEqual(safety["status"], "safe-under-contracts")
                self.assertGreater(safety["safe_period_ticks"], 0)
                self.assertEqual(safety["state_case_count"], 1)
                self.assertEqual(safety["diagnostics"], [])
                self.assertEqual(cycle, [])
            self.assertNotIn("recommended_period", safety)
        # A matching port shape must NOT grant the FF contract to a timed device.
        # A timed memory's endpoint keeps to the clock from outside, and its
        # handshake with the requester is covered state by state: a clock is
        # certified, and it also covers what the compact core needs.
        safety = run("--core", "advanced", "--memory", "sram", "--safe-clock")["safe_clock"]
        self.assertEqual(safety["status"], "safe-under-contracts")
        self.assertEqual(safety["safe_period_ticks"], 102)
        self.assertEqual(safety["checked_state_cases"], safety["state_case_count"])
        self.assertTrue(any("timed memory's clocked handshake" in e for e in safety["state_evidence"]))
        timed = run("--core", "advanced", "--memory", "sram", "--program", "20", "--period", "auto")
        self.assertTrue(timed["verified"])
        self.assertEqual(timed["setup_violations"], 0)
        # Every handshake state the gate-level system passes through is one
        # the clock analysis went through.
        for memory in ("ff", "sram", "dram"):
            seen = run("--core", "advanced", "--memory", memory, "--program", "20", "--fidelity", "reference",
                       "--period", "auto", "--check-handshake-states")
            self.assertTrue(seen["verified"])
            self.assertEqual(seen["setup_violations"], 0)
            self.assertEqual(seen["handshake_states_checked"], 2 * seen["cycles"])
        run("--core", "advanced", "--program", "20", "--check-handshake-states", okay=False)
        # The candidate-clock mode is retired; its option is refused.
        run("--core", "single", "--timing", okay=False)
        run("--safe-clock", "--fidelity", "behavioral", okay=False)

    def test_state_aware_prediction_configurations(self):
        for policy in ("not-taken", "taken", "backward-taken", "one-bit", "two-bit"):
            report = run("--core", "advanced", "--safe-clock", "--policy", policy)
            safe = report["safe_clock"]
            self.assertEqual(safe["status"], "safe-under-contracts")
            self.assertEqual(safe["checked_state_cases"], safe["state_case_count"])
            self.assertEqual(safe["state_case_count"], 36)
            self.assertGreater(safe["safe_period_ticks"], 0)
            # A structural execution is a regression of the derived bound, not
            # the source of the all-state guarantee. Use the returned period.
            result = run("--core", "advanced", "--fidelity", "reference", "--policy", policy,
                         "--program", 20, "--period", safe["safe_period_ticks"],
                         "--allocation-limit-mib", 512)
            self.assertTrue(result["verified"])

    def test_safe_period_structural_corpus(self):
        safety = run("--core", "advanced", "--safe-clock")["safe_clock"]
        self.assertEqual(safety["status"], "safe-under-contracts")
        for program in range(1, 23):
            with self.subTest(program=program):
                result = run("--core", "advanced", "--fidelity", "reference", "--program", program,
                             "--period", safety["safe_period_ticks"], "--allocation-limit-mib", 512)
                self.assertTrue(result["verified"])

    def test_elf_retirement_and_delayed_memory(self):
        fixture = Path(__file__).parent.parent / "fixtures/rv32i/external-smoke/self-check.elf"
        for core in ("single", "baseline", "advanced"):
            row = run("--core", core, "--elf", fixture)
            self.assertTrue(row["verified"])
            self.assertEqual(row["retired_instructions"], 19)
        delayed = run("--program", 21, "--memory", "sram", "--period", 394)
        self.assertTrue(delayed["verified"])
        self.assertGreater(delayed["observations"]["IMEM_RESPONSE_WAIT"], 0)
        self.assertGreater(delayed["observations"]["DMEM_RESPONSE_WAIT"], 0)

    def test_all_shared_programs(self):
        for core in ("single", "baseline", "advanced"):
            for program in range(1, 23):
                with self.subTest(core=core, program=program):
                    row = run("--core", core, "--program", program)
                    self.assertTrue(row["verified"])
                    self.assertEqual(row["instruction_attempts"],
                                     row["retired_instructions"] + row["terminal_attempts"])

    def test_policies_and_determinism(self):
        # Ready wrong-path responses drain on the redirect edge itself.
        for policy, cycles, misses in (("not-taken", 85, 15), ("taken", 57, 1),
                                      ("backward-taken", 57, 1), ("one-bit", 59, 2), ("two-bit", 61, 3)):
            a = run("--policy", policy)
            b = run("--policy", policy, "--no-verify", "--period", 8000)
            self.assertEqual(a["cycles"], cycles)
            self.assertEqual(a["observations"]["BRANCH_MISS"], misses)
            for k in ("cycles", "observations", "processed_events", "workload_fnv1a64", "final_state_fnv1a64"):
                self.assertEqual(a[k], b[k])

    def test_generated_and_windows(self):
        for core in ("single", "baseline", "advanced"):
            for workload in ("dependent", "alternating", "nested"):
                self.assertTrue(run("--core", core, "--workload", workload, "--iterations", 12)["verified"])
            # Generated dependent loop starts at PC 12; terminal is PC 32.
            whole = run("--core", core, "--workload", "dependent", "--iterations", 4)
            region = run("--core", core, "--workload", "dependent", "--iterations", 4,
                         "--start-pc", 12, "--stop-pc", 32)
            self.assertEqual(region["retired_instructions"], 20)
            self.assertEqual(region["terminal_attempts"], 0)
            self.assertEqual(region["whole_program_cycles"], whole["cycles"])
            self.assertLess(region["cycles"], whole["cycles"])

    def test_invalid_inputs(self):
        for args in (("--period", 0), ("--period", 3), ("--period", "10junk"),
                     ("--entries", 3), ("--core", "baseline", "--policy", "taken"),
                     ("--core", "single", "--memory", "sram"), ("--stop-pc", 8),
                     ("--max-events", 5), ("--max-cycles", 1), ("--start-pc", 0xfffffffc)):
            run(*args, okay=False)


if __name__ == "__main__":
    unittest.main()
