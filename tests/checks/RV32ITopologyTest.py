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
    def test_write_buffers(self):
        # Buffered writes reach memory after the program stops; the run must
        # still match the oracle and the final memory, in fewer cycles.
        plain = run("D-ID", "dram", "--workload", "sort", "--size", "16", "--iterations", "2")
        buffered = run("D-ID", "dram", "--workload", "sort", "--size", "16", "--iterations", "2",
                       "--dl1", "write_buffer=2", "--l2", "write_buffer=2")
        self.assertTrue(plain["verified"] and buffered["verified"])
        self.assertLess(buffered["whole_program_cycles"], plain["whole_program_cycles"])
        # Write-back with write-allocate: dirty lines are flushed after the
        # program stops; the memory must still match the oracle.
        back = run("D-ID", "dram", "--workload", "sort", "--size", "16", "--iterations", "2",
                   "--dl1", "write_policy=back,write_miss=allocate", "--l2", "write_policy=back,write_miss=allocate")
        self.assertTrue(back["verified"])
        self.assertLess(back["whole_program_cycles"], plain["whole_program_cycles"])

    def test_overlapping_images_are_refused(self):
        # One shared memory holds code and data at their own addresses; a
        # workload whose code and data collide must not run (program 19's
        # code reaches 0x108 and its data starts at 0x100; load-use unrolls
        # its loads, so 340 elements of code reach its input at 0x1000).
        env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE="embedded")
        for topology in ("A", "B"):
            for workload in (("--program", "19"), ("--workload", "load-use", "--size", "340", "--iterations", "1")):
                with self.subTest(topology=topology, workload=workload):
                    p = subprocess.run([BINARY, "--core", "advanced", *workload, "--memory", "dram",
                                        "--topology", topology, "--period", "4104", "--allocation-limit-mib", "2048"],
                                       capture_output=True, text=True, timeout=300, env=env)
                    self.assertNotEqual(p.returncode, 0)
                    self.assertIn("overlap at byte address", p.stderr)

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

    def test_cached_and_shared_roots_have_a_certified_clock(self):
        # B covers the arbiter; D-ID covers both L1s, the arbiter and the L2.
        # The analysis certifies them at the core's clock: the caches hand
        # their clock on through zero-delay wires, the arbiter takes it
        # straight from its I port (no gate in the clock path).
        for topology in ("B", "D-ID"):
            with self.subTest(topology=topology):
                clock = run(topology, "dram", "--program", "20", "--safe-clock")["safe_clock"]
                self.assertEqual(clock["status"], "safe-under-contracts", clock["diagnostics"])
                self.assertEqual(clock["safe_period_ticks"], 4104)
                self.assertEqual(clock["diagnostics"], [])
                carried = [a for a in clock["assumptions"] if "Rewire(s) pass the clock on" in a]
                self.assertEqual(len(carried), 1)

    def test_program_through_sram_memory(self):
        result = run("D-ID", "sram")
        self.assertTrue(result["verified"])


if __name__ == "__main__":
    unittest.main()
