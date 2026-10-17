"""The five memory topologies of the cache campaign run a program correctly.

Each run is checked against the instruction oracle and the final memory image
by the experiment itself; this test also checks the traffic at each boundary.
"""
import json
import os
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)


def run(topology, memory="dram", *workload):
    env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE="embedded")
    p = subprocess.run([BINARY, "--core", "advanced", *(workload or ("--program", "20")), "--memory", memory,
                        "--topology", topology, "--period", "4104", "--allocation-limit-mib", "2048"],
                       capture_output=True, text=True, timeout=300, env=env)
    if p.returncode:
        raise AssertionError(p.stderr)
    return json.loads(p.stdout)


class Topologies(unittest.TestCase):
    def test_every_topology_verifies(self):
        # A sort of eight words: loads, stores and a loop, through every path.
        for topology in ("A", "B", "C-ID", "D-0", "D-ID"):
            with self.subTest(topology=topology):
                result = run(topology, "dram", "--workload", "sort", "--size", "8", "--iterations", "2")
                self.assertTrue(result["completed"])
                self.assertTrue(result["verified"])
                seen = result["observations"]
                caches = [name for name in ("IL1", "DL1", "L2") if name + "_RESPONSE_WORDS" in seen]
                expected = {"A": [], "B": [], "C-ID": ["IL1", "DL1"], "D-0": ["L2"], "D-ID": ["IL1", "DL1", "L2"]}
                self.assertEqual(sorted(caches), sorted(expected[topology]))
                self.assertEqual("ARBITER_RESPONSE_WORDS" in seen, topology != "A")
                # Caches fill whole lines with bursts: four words per burst,
                # and no single-word read below a cache.
                for name in caches:
                    self.assertEqual(seen.get(name + "_READS", 0), 0)
                    self.assertGreater(seen.get(name + "_BURST_READS", 0), 0)
                    # Four words per burst, one per write; a burst begun at
                    # the end of the run may not have finished.
                    full = 4 * seen[name + "_BURST_READS"] + seen.get(name + "_WRITES", 0)
                    self.assertLessEqual(seen[name + "_RESPONSE_WORDS"], full)
                    self.assertGreater(seen[name + "_RESPONSE_WORDS"], full - 4)
                # Write-through: the data L1 sends every store down.
                if "DL1" in caches:
                    self.assertGreater(seen.get("DL1_WRITES", 0), 0)
                if topology in ("C-ID", "D-ID"):
                    self.assertEqual(seen.get("ARBITER_READS", 0), 0)

    def test_program_through_sram_memory(self):
        result = run("D-ID", "sram")
        self.assertTrue(result["verified"])


if __name__ == "__main__":
    unittest.main()
