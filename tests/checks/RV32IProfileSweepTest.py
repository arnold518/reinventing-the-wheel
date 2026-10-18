"""CPU programs under the realistic timing profiles.

The native suite is written in `unit` ticks. This test covers `embedded` and
`desktop`: every core runs a sample of the shared programs at the clock the
analyzer certifies for it in that profile, checked against the instruction
oracle and the final memory image, with no setup violation.
"""
import json
import os
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)
PROGRAMS = (1, 9, 16, 19, 20)
SYSTEMS = (("single", "ff"), ("baseline", "ff"), ("advanced", "ff"), ("advanced", "sram"), ("advanced", "dram"))


def run(profile, core, memory, program):
    env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE=profile)
    p = subprocess.run([BINARY, "--core", core, "--memory", memory, "--program", str(program),
                        "--period", "auto", "--allocation-limit-mib", "1024"],
                       capture_output=True, text=True, timeout=300, env=env)
    if p.returncode:
        raise AssertionError(f"{profile} {core} {memory} program {program}: {p.stderr[-500:]}")
    return json.loads(p.stdout)


def certified():
    p = subprocess.run([BINARY, "--list-certified-periods"], capture_output=True, text=True, timeout=60, check=True)
    return {(e["profile"], e["core"]): e["period_ticks"] for e in json.loads(p.stdout)}


class ProfileSweep(unittest.TestCase):
    def test_stored_certified_periods_match_the_analysis(self):
        # The fixtures use the stored table instead of running the analysis;
        # it must give what --period auto gives, in every profile.
        stored = certified()
        self.assertEqual({profile for profile, _ in stored}, {"unit", "embedded", "desktop"})
        for (profile, core), period in sorted(stored.items()):
            with self.subTest(profile=profile, core=core):
                self.assertEqual(run(profile, core, "ff", 20)["period_ticks"], period)

    def test_programs_at_certified_clocks(self):
        for profile in ("embedded", "desktop"):
            for core, memory in SYSTEMS:
                for program in PROGRAMS:
                    with self.subTest(profile=profile, core=core, memory=memory, program=program):
                        result = run(profile, core, memory, program)
                        self.assertTrue(result["completed"])
                        self.assertTrue(result["verified"])
                        self.assertEqual(result["setup_violations"], 0)
                        self.assertGreater(result["period_ticks"], 0)


if __name__ == "__main__":
    unittest.main()
