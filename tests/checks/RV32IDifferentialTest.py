"""Bounded cycle-by-cycle differential checks through the benchmark runner.

Each process owns one circuit, records <=1024 cycles, and has a 512 MiB cap.
Seeded ELF images use the normal loader. No simulator internals seed results.
"""
import hashlib
import json
import random
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BINARY = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[2]
POLICIES = ("not-taken", "taken", "backward-taken", "one-bit", "two-bit")
sys.path.insert(0, str(ROOT / "tools/rv32i-performance"))
from experiment import signature


def first_difference(expected, actual):
    """Report the first cycle/field; ordering is part of equivalence."""
    for cycle, (a, b) in enumerate(zip(expected, actual)):
        for key in sorted(a.keys() | b.keys()):
            if a.get(key) != b.get(key):
                return f"cycle {cycle}, {key}: behavioral={a.get(key)}, structural={b.get(key)}"
    if len(expected) != len(actual):
        return f"trace lengths: behavioral={len(expected)}, structural={len(actual)}"
    return None


def immediate(rd, rs, value, function=0, opcode=0x13):
    assert 0 <= rd < 32 and 0 <= rs < 32 and -2048 <= value <= 2047
    return (value & 4095) << 20 | rs << 15 | function << 12 | rd << 7 | opcode


def store(rs, base, offset, size=2):
    value = offset & 4095
    return (value >> 5) << 25 | rs << 20 | base << 15 | size << 12 | (value & 31) << 7 | 0x23


def branch(a, b, offset, function=0):
    assert offset % 4 == 0 and -4096 <= offset <= 4094
    v = offset & 8191
    return ((v >> 12) << 31 | ((v >> 5) & 63) << 25 | b << 20 | a << 15
            | function << 12 | ((v >> 1) & 15) << 8 | ((v >> 11) & 1) << 7 | 0x63)


def constant(rd, value):
    value &= 0xffffffff
    lo = value & 4095
    return [((value + 2048) & 0xfffff000) | rd << 7 | 0x37,
            immediate(rd, rd, lo - 4096 if lo & 2048 else lo)]


def seeded_program(seed):
    rng = random.Random(seed)
    words = []
    for reg, value in enumerate((0, 1, 0x7fffffff, 0x80000000, 0xffffffff, 31, 32, rng.getrandbits(32)), 1):
        words += constant(reg, value)
    words += constant(20, 0x1000)
    # Competing forwarding sources for x1: the youngest producer must win.
    words += [immediate(1, 0, 7), immediate(1, 1, -3), immediate(2, 1, 9)]
    # Every ALU operation appears, with seed-dependent sources and distances.
    for function, upper in ((0, 0), (0, 32), (1, 0), (2, 0), (3, 0),
                            (4, 0), (5, 0), (5, 32), (6, 0), (7, 0)):
        a, b, dest = rng.randrange(1, 9), rng.randrange(1, 9), rng.randrange(0, 9)
        words.append(upper << 25 | b << 20 | a << 15 | function << 12 | dest << 7 | 0x33)
        words += [immediate(9, 9, 1)] * rng.randrange(3)
        words += [immediate(3, dest, rng.choice((-2048, -1, 0, 1, 2047)))]
    # Every byte lane, signed/unsigned loads, halfwords, load->store and x0.
    for offset in range(4):
        words += [store(3, 20, offset, 0), immediate(5, 20, offset, 0, 3),
                  immediate(6, 20, offset, 4, 3), store(5, 20, 8), immediate(0, 5, 1)]
    words += [store(3, 20, 2, 1), immediate(5, 20, 2, 1, 3),
              immediate(6, 20, 2, 5, 3), immediate(0, 20, 8, 2, 3)]
    # Load-dependent taken branch: squash both a store and illegal instruction.
    words += [immediate(5, 20, 8, 2, 3), branch(5, 5, 12), store(3, 20, 240), 0xffffffff]
    # Back-to-back redirects, including a freshly produced odd JALR target.
    words += [0x00c005ef, store(3, 20, 240), 0xffffffff]  # jal x11,+12
    target = 4 * (len(words) + 4) + 1
    words += [immediate(12, 0, target), immediate(11, 12, 0, 0, 0x67),
              store(3, 20, 240), 0xffffffff]
    # Independently self-check the protected wrong-path location and x0.
    words += [immediate(5, 20, 240, 2, 3), branch(5, 0, 8), 0xffffffff,
              immediate(0, 0, 123), branch(0, 0, 8), 0xffffffff]
    words += constant(21, 0x3ffc0)
    words += [immediate(22, 0, 1), store(22, 21, 0), 0x00100073]
    return words


def elf_bytes(words):
    code = struct.pack("<" + "I" * len(words), *words)
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    header = struct.pack("<16sHHIIIIIHHHHHH", ident, 2, 243, 1, 0, 52, 0, 0, 52, 32, 1, 0, 0, 0)
    segment = struct.pack("<IIIIIIII", 1, 0x100, 0, 0, len(code), len(code), 5, 0x100)
    return header + segment + bytes(0x100 - len(header) - len(segment)) + code


class Differential(unittest.TestCase):
    periods = {}

    def run_core(self, *args, okay=True):
        result = subprocess.run([str(BINARY), *map(str, args), "--allocation-limit-mib", "512"],
                                capture_output=True, text=True, timeout=90)
        if not okay:
            self.assertNotEqual(result.returncode, 0)
            return result
        self.assertEqual(result.returncode, 0, result.stderr[-4000:])
        return json.loads(result.stdout)

    def pair(self, core, *workload, policy="not-taken", entries=16):
        config = ["--core", core]
        if core == "advanced":
            config += ["--policy", policy, "--entries", entries]
        key = core, policy, entries
        if key not in self.periods:
            timing = self.run_core(*config, "--safe-clock", "--binary-startup")["safe_clock"]
            self.assertEqual(timing["status"], "safe-under-contracts")
            self.assertTrue(timing["binary_startup_verified"])
            self.periods[key] = timing["safe_period_ticks"]
        args = [*config, *workload, "--period", self.periods[key], "--max-cycles", 1024,
                "--max-events", 80000000, "--trace-cycles"]
        compact = self.run_core(*args, "--fidelity", "behavioral")
        reference = self.run_core(*args, "--fidelity", "reference")
        for row in (compact, reference):
            self.assertTrue(row["verified"])
            self.assertTrue(row["completed"])
            self.assertEqual(row["cycle_trace_schema"], 1)
            self.assertEqual(len(row["cycle_trace"]), row["whole_program_cycles"])
            self.assertEqual(sum(x["retire.valid"] for x in row["cycle_trace"]), row["instruction_attempts"])
        self.assertIsNone(first_difference(compact["cycle_trace"], reference["cycle_trace"]),
                          f"{core}/{policy}/{entries}/{workload}: "
                          + str(first_difference(compact["cycle_trace"], reference["cycle_trace"])))
        self.assertEqual(signature(compact), signature(reference))
        print(f"matched {core}/{policy}/{entries} {workload}: {compact['cycles']} cycles", flush=True)
        return compact

    def test_seeded_dependencies_and_redirects(self):
        with tempfile.TemporaryDirectory(prefix="ritw-differential-") as directory:
            for seed in (1, 0x12345678, 0xc001d00d, 0xffffffff):
                image = elf_bytes(seeded_program(seed))
                self.assertEqual(image, elf_bytes(seeded_program(seed)))
                path = Path(directory) / f"seed-{seed:08x}.elf"
                path.write_bytes(image)
                for core in ("single", "baseline", "advanced"):
                    with self.subTest(core=core, seed=seed):
                        self.pair(core, "--elf", path)

    def test_fault_priority(self):
        for program in (9, 11, 12, 13, 14, 15, 16):
            for core in ("single", "baseline", "advanced"):
                with self.subTest(core=core, program=program):
                    row = self.pair(core, "--program", program)
                    self.assertEqual(row["terminal_attempts"], 1)
                    if program in (11, 12):
                        requests = [cycle for cycle in row["cycle_trace"] if cycle["DMEM_REQ_VALID"]]
                        if core == "advanced":
                            # The advanced stage rejects misalignment before issue.
                            self.assertEqual(requests, [])
                        else:
                            # Zero-wait cores probe the aligned word while checking
                            # alignment locally; a fault must never enable a write.
                            self.assertTrue(requests)
                            self.assertTrue(all(cycle["DMEM_REQ_OP"] == 0
                                                and cycle["DMEM_REQ_ADDR"] == 0x100
                                                for cycle in requests))

    def test_predictor_history_and_aliases(self):
        for policy in POLICIES:
            with self.subTest(policy=policy, workload="branch-pattern"):
                # 8 equal outcomes, then a reversal; enough updates to saturate.
                self.pair("advanced", "--workload", "branch-pattern", "--pattern", "bursts",
                          "--iterations", 12, "--size", 4, policy=policy, entries=16)
            with self.subTest(policy=policy, workload="branch-alias"):
                self.pair("advanced", "--workload", "branch-alias", "--branch-spacing", 4,
                          "--iterations", 5, "--size", 4, policy=policy, entries=4)

    def test_compiled_stack_and_calls(self):
        path = ROOT / "tests/fixtures/rv32i/differential/compiled.elf"
        expected = (path.parent / "compiled.elf.sha256").read_text().split()[0]
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), expected)
        for core in ("single", "baseline", "advanced"):
            with self.subTest(core=core):
                self.pair(core, "--elf", path)

    def test_trace_contract_and_comparator(self):
        # Same totals and payloads in a different cycle must still fail.
        a = [{"cycle": 0, "retire.valid": 0}, {"cycle": 1, "retire.valid": 1, "retire.pc": 0}]
        b = [{"cycle": 0, "retire.valid": 1, "retire.pc": 0}, {"cycle": 1, "retire.valid": 0}]
        self.assertIn("cycle 0", first_difference(a, b))
        self.assertIsNone(first_difference(a, a))
        self.assertIn("trace lengths", first_difference(a, a[:1]))
        self.assertIn("DMEM_REQ_WRITE_DATA", first_difference(
            [{"DMEM_REQ_WRITE_DATA": 1}], [{"DMEM_REQ_WRITE_DATA": 2}]))
        self.run_core("--trace-cycles", okay=False)
        self.run_core("--trace-cycles", "--max-cycles", 4097, okay=False)
        self.run_core("--trace-cycles", "--max-cycles", 16, "--safe-clock", okay=False)
        row = self.run_core("--core", "single", "--program", 1, "--trace-cycles",
                            "--max-cycles", 2, "--allow-incomplete")
        self.assertEqual(len(row["cycle_trace"]), 2)
        self.assertFalse(row["verified"])
        plain = self.run_core("--core", "single", "--program", 1,
                              "--max-cycles", 2, "--allow-incomplete")
        self.assertNotIn("cycle_trace", plain)
        self.assertEqual(signature(row), signature(plain))
        self.assertEqual(row["processed_events"], plain["processed_events"])


if __name__ == "__main__":
    unittest.main()
