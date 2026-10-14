"""Validate the actual Markdown answer sheet against compiled reference code."""

from pathlib import Path
from functools import lru_cache
import json
import random
import re
import subprocess
import sys
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "tools"))
from mkdocs_rv32i import SOURCE, PROGRAM, source_listing, render_program

PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else None
CXX = sys.argv.pop(1) if len(sys.argv) > 1 else None
SIMULATOR_LIBRARY = sys.argv.pop(1) if len(sys.argv) > 1 else None
DOC_ROOT = REPOSITORY / "site-docs/rv32i"
DOCUMENTS = {str(path.relative_to(DOC_ROOT)): path.read_text()
             for path in sorted(DOC_ROOT.rglob("*.md"))}
DOCUMENT = "\n\n".join(DOCUMENTS.values())
DEFERRED_CORE = (REPOSITORY / "docs/reference/rv32i-single-cycle-documentation-notes.md").read_text()
PROGRAM_COUNT = int(re.search(r"\bProgramCount\s*=\s*(\d+)",
    (REPOSITORY / "include/rv32i/RV32IValidationConfig.hpp").read_text())[1])
PROGRAM_DOCUMENTS = {int(match[1]): source for path, source in DOCUMENTS.items()
    if (match := re.fullmatch(r"4\.2/programs/program-(\d+)\.md", path))}
ENTRIES = {}
for path, source in DOCUMENTS.items():
    if "/instructions/" not in path or path.endswith("/index.md"):
        continue
    for match in re.finditer(r"(?ms)^## ([A-Z]+) — (.*?) \{#([a-z]+)\}\n(.*?)(?=^## |\Z)", source):
        name, title, anchor, content = match.groups()
        if name in ENTRIES:
            raise AssertionError(f"Duplicate instruction: {name}")
        ENTRIES[name] = (path, title, anchor, content)


def table(marker):
    section = DOCUMENT.split(f"<!-- {marker} -->\n", 1)[1]
    lines = []
    for line in section.splitlines()[2:]:
        if not line.startswith("|"):
            break
        lines.append([cell.strip().strip("`") for cell in line.strip("|").split("|")])
    return lines


def probe(mode, rows):
    result = subprocess.run([PROBE, mode], input="\n".join(rows) + "\n",
                            text=True, capture_output=True, check=True)
    output = [line.split() for line in result.stdout.splitlines()]
    if len(output) != len(rows):
        raise AssertionError("Probe omitted results")
    return output


@lru_cache(maxsize=1)
def program_fixtures():
    result = subprocess.run([PROBE, "program"],
        input="\n".join(map(str, range(1, PROGRAM_COUNT + 1))) + "\n",
        text=True, capture_output=True, check=True)
    return {fixture["number"]: fixture for line in result.stdout.splitlines()
            if (fixture := json.loads(line))}


class RV32IReferenceDocumentationTest(unittest.TestCase):
    def test_instruction_inventory_and_masks(self):
        encodings = table("rv32i-encodings")
        enum = (REPOSITORY / "include/rv32i/RV32IInstruction.hpp").read_text()
        expected = re.search(r"enum class RV32IInstruction \{(.*?)\};", enum, re.S)[1]
        names = re.findall(r"\b([A-Z][A-Z0-9]+),", expected)
        self.assertEqual([row[0] for row in encodings], [n for n in names if n != "INVALID"])
        self.assertEqual(len(encodings), 40)
        patterns = [(name, fmt, int(mask, 16), int(match, 16)) for name, fmt, mask, match in encodings]
        for name, _, mask, match in patterns:
            self.assertEqual(match & mask, match, name)
        for i, (name, _, mask, match) in enumerate(patterns):
            for other, _, mask2, match2 in patterns[i + 1:]:
                self.assertNotEqual((match ^ match2) & (mask & mask2), 0, (name, other))
        # Entire opcode/funct3/funct7 selector product, fixed rd=5/rs1=6/rs2=7.
        words = [op | (5 << 7) | (f3 << 12) | (6 << 15) | (7 << 20) | (f7 << 25)
                 for op in range(128) for f3 in range(8) for f7 in range(128)]
        rng = random.Random(0x321)
        words += [rng.getrandbits(32) for _ in range(4096)]
        # Exercise ignored operand/immediate bits for every individual row.
        words += [match | (rng.getrandbits(32) & (~mask & 0xffffffff))
                  for _, _, mask, match in patterns for _ in range(64)]
        actual = probe("decode", [f"{word:x}" for word in words])
        for word, decoded in zip(words, actual):
            matches = [(name, fmt) for name, fmt, mask, match in patterns if word & mask == match]
            self.assertEqual(decoded[:2], list(matches[0]) if matches else ["INVALID", "Invalid"], hex(word))

    def test_decoded_examples(self):
        rows = table("rv32i-decode-examples")
        actual = probe("decode", [row[0] for row in rows])
        for row, result in zip(rows, actual):
            self.assertEqual(result, row[1:3] + ["Legal"] + row[3:], row[0])

    def test_named_selector_tables(self):
        encodings = table("rv32i-encodings")
        for marker, opcodes in [("rv32i-alu-selectors", [0x33, 0x13]),
                                ("rv32i-memory-selectors", [0x03, 0x23, 0x63])]:
            rows = table(marker)
            self.assertEqual([int(row[0], 2) for row in rows], list(range(8)))
            for row in rows:
                for opcode, selection in zip(opcodes, row[1:]):
                    expected = {name for name, _, mask, match in encodings
                                if int(match, 16) & 0x7f == opcode
                                and (int(match, 16) >> 12) & 7 == int(row[0], 2)}
                    self.assertEqual(set(selection.split(" / ")) if selection != "Invalid" else set(),
                                     expected, (marker, row[0], opcode))

    def test_readable_alu_examples(self):
        encodings = {name: int(match, 16) for name, _, _, match in table("rv32i-encodings")}
        rows = []
        for marker, register in [("rv32i-register-reference", True),
                                 ("rv32i-immediate-reference", False)]:
            examples = table(marker)
            self.assertEqual(len(examples), 10 if register else 9)
            for syntax, _, example in examples:
                match = re.fullmatch(r"([a-z]+) x5, x6, (x7|-?\d+)` → `x5=(0x[0-9A-F]+|\d+)", example)
                self.assertIsNotNone(match, example)
                name, operand, result = match.groups()
                self.assertEqual(name.upper(), syntax.split()[0])
                word = encodings[name.upper()] | (5 << 7) | (6 << 15)
                if register:
                    self.assertEqual(operand, "x7")
                    word |= 7 << 20
                else:
                    immediate = int(operand)
                    self.assertTrue(0 <= immediate <= 31 if name in ("slli", "srli", "srai")
                                    else -2048 <= immediate <= 2047)
                    word |= (immediate & 0xfff) << 20
                rows.append((word, name.upper(), int(result, 0)))
        actual = probe("step", [f"{word:x} a 3" for word, _, _ in rows])
        for (_, name, expected), result in zip(rows, actual):
            self.assertEqual(result[0], name)
            self.assertEqual(int(result[1], 16), expected, name)
            self.assertEqual([int(v, 16) for v in result[2:4]], [0x44, 0x80ff7f01], name)
            self.assertEqual(result[4], "Run", name)

    def test_readable_reference_covers_all_instructions(self):
        self.assertEqual(len(ENTRIES), 40)
        self.assertEqual(set(ENTRIES), {row[0] for row in table("rv32i-encodings")})
        fields = ["Purpose", "Assembly syntax", "Encoding and decoding", "Operands", "Operation",
                  "Exceptions and special cases", "Example"]
        encodings = {row[0]: row[1:] for row in table("rv32i-encodings")}
        index = DOCUMENTS["4.1/instructions/index.md"]
        for name, (path, title, anchor, content) in ENTRIES.items():
            self.assertEqual(anchor, name.lower())
            self.assertTrue(title)
            self.assertEqual(re.findall(r"(?m)^### (.*)$", content), fields, name)
            self.assertIn(f"[{name}]({Path(path).name}#{anchor})", index)
            metadata = dict(re.findall(r"(?m)^\| (Format|Mask|Match) \| `?([^`| ]+)`? \|$", content))
            self.assertEqual([metadata[key] for key in ("Format", "Mask", "Match")], encodings[name])
            self.assertIn(f"<!-- rv32i-operation {name} -->", content)
            self.assertIn(f"<!-- rv32i-example {name} -->", content)

    def test_entry_bit_layouts_match_masks(self):
        for name, (_, _, _, content) in ENTRIES.items():
            mask = int(re.search(r"^\| Mask \| `(0x[0-9A-F]+)`", content, re.M)[1], 16)
            match = int(re.search(r"^\| Match \| `(0x[0-9A-F]+)`", content, re.M)[1], 16)
            layout = re.search(r"(?m)^(\| `31(?::\d+)?` \|[^\n]*)\n\|[^\n]*\n(\|[^\n]*)", content)
            self.assertIsNotNone(layout, name)
            positions, values = [[cell.strip().strip("`") for cell in line.strip("|").split("|")]
                                 for line in layout.groups()]
            self.assertEqual(len(positions), len(values))
            next_bit, diagram_mask, diagram_match = 31, 0, 0
            for position, value in zip(positions, values):
                parts = [int(n) for n in position.split(":")]
                high, low = parts[0], parts[-1]
                self.assertEqual(high, next_bit, name)
                self.assertLessEqual(low, high)
                next_bit = low - 1
                if re.fullmatch("[01]+", value):
                    self.assertEqual(len(value), high - low + 1, (name, position))
                    diagram_mask |= ((1 << len(value)) - 1) << low
                    diagram_match |= int(value, 2) << low
            self.assertEqual(next_bit, -1, name)
            self.assertEqual((diagram_mask, diagram_match), (mask, match), name)

    def test_entry_examples_match_compiled_engine(self):
        answers = {row[0]: row for row in table("rv32i-step-examples")}
        for name, (_, _, _, content) in ENTRIES.items():
            with self.subTest(instruction=name):
                row = answers[name]
                word = re.search(r"Instruction word: `(0x[0-9A-F]+)`", content)[1]
                self.assertEqual(word, row[1])
                section = content.split(f"<!-- rv32i-example {name} -->\n", 1)[1]
                values = {}
                for line in section.splitlines()[2:]:
                    if not line.startswith("|"):
                        break
                    key, before, after = [s.strip().strip("`") for s in line.strip("|").split("|")]
                    values[key] = (before, after)
                self.assertEqual(values["PC"], ("0x00000040", row[5]))
                self.assertEqual(values["Status"], ("Run", row[7]))
                if "x5" in values:
                    self.assertEqual(values["x5"], ("0x00000000", row[4]))
                for reg, column in (("x6", 2), ("x7", 3)):
                    if reg in values:
                        self.assertEqual(values[reg], (row[column], row[column]))
                if name in ("LB", "LH", "LW", "LBU", "LHU", "SB", "SH", "SW"):
                    self.assertEqual(values["data word at 0x100"], ("0x80FF7F01", row[6]))

    def test_operation_blocks_against_compiled_engine(self):
        # Execute the actual documentation snippets with the documented helper
        # contract. This is independent of the production decoder/ALU helpers.
        examples = table("rv32i-step-examples") + table("rv32i-edge-examples")
        cases = [(row[0], int(row[1], 16), int(row[2], 16), int(row[3], 16))
                 for row in examples if row[0] != "INVALID"]
        rng = random.Random(0x31)
        for name, word, _, _ in cases[:40]:
            for _ in range(16):
                cases.append((name, word, rng.getrandbits(32), rng.getrandbits(32)))
        decoded = probe("decode", [f"{word:x}" for _, word, _, _ in cases])
        actual = probe("step", [f"{word:x} {a:x} {b:x}" for _, word, a, b in cases])

        class Trap(Exception):
            pass

        for (name, word, a, b), fields, expected in zip(cases, decoded, actual):
            with self.subTest(instruction=name, word=hex(word), a=hex(a), b=hex(b)):
                x = [0] * 32
                x[6], x[7] = a, b
                memory = {0x100 + i: value for i, value in enumerate([1, 0x7f, 0xff, 0x80])}
                rd, rs1, rs2, immediate, shamt = map(int, fields[3:])
                def u32(value):
                    return value & 0xffffffff
                def sext(value, width):
                    value &= (1 << width) - 1
                    return value - (1 << width) if value & (1 << (width - 1)) else value
                def trap(cause):
                    raise Trap(cause)
                def check_target(address):
                    if address % 4:
                        trap("InstructionAddressMisaligned")
                def check_memory(address, width, store=False):
                    prefix = "Store" if store else "Load"
                    if address % width:
                        trap(prefix + "AddressMisaligned")
                    if address < 0 or address + width > 0x40000:
                        trap(prefix + "AccessFault")
                def read(address, width):
                    check_memory(address, width)
                    return sum(memory.get(address + i, 0) << (8 * i) for i in range(width))
                def store(address, value, width):
                    check_memory(address, width, True)
                    for i in range(width):
                        memory[address + i] = (value >> (8 * i)) & 255
                def write_rd(value):
                    if rd:
                        x[rd] = u32(value)
                environment = {"__builtins__": {}, "x": x, "pc": 0x40, "rd": rd,
                    "rs1": rs1, "rs2": rs2, "imm": immediate, "shamt": shamt,
                    "halted": False, "u32": u32, "s32": lambda v: sext(v, 32),
                    "sext": sext, "trap": trap, "check_target": check_target, "write_rd": write_rd}
                for bits in (8, 16, 32):
                    environment[f"read{bits}"] = lambda address, width=bits // 8: read(address, width)
                    environment[f"store{bits}"] = lambda address, value, width=bits // 8: store(address, value, width)
                content = ENTRIES[name][3]
                operation = re.search(rf"<!-- rv32i-operation {name} -->\n```python\n(.*?)\n```", content, re.S)[1]
                status = "Run"
                try:
                    exec(compile(operation, f"{ENTRIES[name][0]}#{name.lower()}", "exec"), environment)
                    status = "Halt" if environment["halted"] else "Run"
                except Trap as error:
                    status = str(error)
                memory_word = sum(memory.get(0x100 + i, 0) << (8 * i) for i in range(4))
                self.assertEqual([x[5], environment["pc"], memory_word], [int(v, 16) for v in expected[1:4]])
                self.assertEqual(status, expected[4])
                self.assertEqual([x[0], x[6], x[7]], [int(v, 16) for v in expected[5:8]])

    def test_every_one_step_answer(self):
        rows = table("rv32i-step-examples")
        self.assertEqual([r[0] for r in rows], [r[0] for r in table("rv32i-encodings")])
        rows += table("rv32i-edge-examples")
        self.assertEqual(len(rows), 54)
        actual = probe("step", [" ".join(row[1:4]) for row in rows])
        for row, result in zip(rows, actual):
            with self.subTest(instruction=row[0]):
                self.assertEqual(result[0], row[0])
                self.assertEqual([int(v, 16) for v in result[1:4]], [int(v, 16) for v in row[4:7]])
                self.assertEqual(result[4], row[7])
                self.assertEqual([int(v, 16) for v in result[5:]], [0, int(row[2], 16), int(row[3], 16), 1])

    def test_rejection_diagnostics(self):
        section = DOCUMENTS["4.1/formats.md"].split("## Rejection and diagnostics", 1)[1].split("## Decoder output", 1)[0]
        rows = re.findall(r"(?m)^\| `([A-Za-z0-9]+)` \| .*? \| `(0x[0-9A-F]+)`", section)
        self.assertEqual(len(rows), 6)
        actual = probe("decode", [word for _, word in rows])
        for (status, word), result in zip(rows, actual):
            self.assertEqual(result[:3], ["INVALID", "Invalid", status], word)

    def test_pin_table_matches_public_schema(self):
        # Check the published contract, including grouped universal-port rows.
        result = subprocess.run([PROBE, "pins"], text=True, capture_output=True, check=True)
        pins = [tuple(line.split()) for line in result.stdout.splitlines()]
        section = DOCUMENTS["4.4/core.md"].split("### IO pins", 1)[1].split("### Behavior", 1)[0]
        rows = re.findall(r"(?m)^\| (.*?) \| (Input|Output) \| (\d+)(?: each)? \|", section)
        documented = [(name, direction, width) for names, direction, width in rows
                      for name in re.findall(r"`([A-Z_]+)`", names)]
        self.assertEqual(sorted(documented), sorted(pins))
        system = (REPOSITORY / "src/modules/rv32i/RV32ISingleCycleSystem.cpp").read_text()
        function = system.split("void initializeSystemPins", 1)[1].split("} // namespace", 1)[0]
        self.assertEqual(re.findall(r'addPin(?:<\d+>)?\("([A-Z_]+)"', function),
                         ["CLK", "RST", "ENABLE", "PC", "HALTED", "TRAPPED"])

    def test_sources_are_complete_and_safe(self):
        matches = list(SOURCE.finditer(DOCUMENT))
        self.assertEqual(len(matches), 11)
        self.assertTrue(all(match[2].startswith(("include/rv32i/", "src/rv32i/")) for match in matches))
        deferred = list(SOURCE.finditer(DEFERRED_CORE))
        self.assertEqual({match[2] for match in deferred}, {
            "src/modules/rv32i/RV32IReferenceCore.hpp",
            "src/modules/rv32i/RV32IReferenceCore.cpp",
            "src/modules/rv32i/RV32IReferenceSystem.hpp",
            "src/modules/rv32i/RV32IReferenceSystem.cpp",
        })
        for match in matches + deferred:
            code = (REPOSITORY / match[2]).read_text().rstrip()
            listing = source_listing(match)
            self.assertIn(code, "\n".join(line.removeprefix(match[1]) for line in listing.splitlines()))
        forbidden = SOURCE.search("<!-- simulator-source: ../../etc/passwd -->")
        with self.assertRaises(ValueError):
            source_listing(forbidden)

    def test_executable_reference_scope(self):
        reference = DOCUMENTS["4.1/implementation.md"]
        self.assertIn("# § 4.1.4. Executable RV32I Reference", reference)
        for heading in ("State and inputs", "Execution interface", "Running the reference",
                        "Complete behavioral implementation", "Using it as an answer sheet"):
            self.assertIn(f"## {heading}", reference)
        for hardware_detail in ("Core IO pins", "Clock, memory, and unknown-input behavior",
                                "RV32IReferenceCore", "RV32IReferenceSystem",
                                "RV32IZeroWaitMemoryPort", "`WRITEBACK`", "`DECODE.CONTROL`"):
            self.assertNotIn(hardware_detail, reference)
            self.assertIn(hardware_detail, DEFERRED_CORE)

    def test_reference_usage_compiles_and_runs(self):
        reference = DOCUMENTS["4.1/implementation.md"]
        code = re.search(r"<!-- rv32i-reference-usage -->\n```cpp\n(.*?)\n```", reference, re.S)[1]
        expected = re.search(r"<!-- rv32i-reference-usage-output -->\n```text\n(.*?)\n```", reference, re.S)[1]
        self.assertIsNotNone(CXX, "Run through CTest to supply the configured compiler and library")
        self.assertIsNotNone(SIMULATOR_LIBRARY)
        with tempfile.TemporaryDirectory(prefix="rv32i-doc-example-") as directory:
            executable = str(Path(directory) / "reference-example")
            # Compile the exact fenced example from stdin, not a copied fixture.
            compiled = subprocess.run([CXX, "-std=c++20", f"-I{REPOSITORY / 'include'}",
                "-x", "c++", "-", "-x", "none", SIMULATOR_LIBRARY, "-o", executable],
                input=code, text=True, capture_output=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([executable], text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, expected + "\n")

    def test_program_rendering_and_links(self):
        program_document = "\n\n".join(PROGRAM_DOCUMENTS.values())
        self.assertEqual(len(PROGRAM.findall(program_document)), PROGRAM_COUNT)
        for scenario, test in PROGRAM.findall(program_document):
            self.assertIn(f"scenario={scenario}&amp;core_fidelity=behavioral", program_document)
            data = {"scenario": scenario, "registeredTest": test, "coreFidelity": "behavioral",
                    "checkpoints": [{"rowIndex": 0, "time": 7001, "index": 7,
                        "label": "I0 ADDI @ 0x00000000", "detail": "x1 < 2 & > 0"}]}
            html = render_program(data, scenario, test)
            self.assertIn('tabindex="0"', html)
            self.assertIn('data-time="7001"', html)
            self.assertIn("x1 &lt; 2 &amp; &gt; 0", html)
            data["checkpoints"][0]["rowIndex"] = 1
            with self.assertRaises(ValueError):
                render_program(data, scenario, test)

    def test_all_program_pages_follow_the_same_pattern(self):
        self.assertEqual(set(PROGRAM_DOCUMENTS), set(range(1, PROGRAM_COUNT + 1)))
        catalogue = DOCUMENTS["4.2/programs/index.md"]
        fields = ["Purpose", "Setup", "Assembly", "Execution", "Expected result", "Simulation and checkpoints"]
        for number, source in PROGRAM_DOCUMENTS.items():
            self.assertTrue(source.startswith(f"# Program {number}: "))
            self.assertEqual(re.findall(r"(?m)^## (.*)$", source), fields, number)
            self.assertIn(f"](program-{number:02}.md)", catalogue)
            self.assertEqual(PROGRAM.findall(source), [(f"rv32i-program{number}",
                f"RV32ISingleCycleSystemTest/program-{number:02}")])
            self.assertIn(f'../../../../simulator/?scenario=rv32i-program{number}&amp;core_fidelity=behavioral', source)
        self.assertIn(f"**{sum(f['expected']['count'] for f in program_fixtures().values())} instruction-attempt checkpoints**", catalogue)

    def test_program_setup_and_final_expectations(self):
        def bytes_from_table(marker):
            result = {}
            for address, values in table(marker):
                if address == "None":
                    self.assertEqual(values, "—")
                    continue
                for offset, byte in enumerate(values.split()):
                    key = str(int(address, 0) + offset)
                    self.assertNotIn(key, result)
                    result[key] = int(byte, 16)
            return result

        for number, fixture in program_fixtures().items():
            with self.subTest(program=number):
                source = PROGRAM_DOCUMENTS[number]
                self.assertEqual(fixture["initialRegisters"], [0] * 32)
                self.assertEqual(fixture["initialPC"], 0)
                self.assertEqual(fixture["capacity"], 256 * 1024)
                self.assertIn(f"| Instruction image | {len(fixture['words'])} words, starting at `0x{fixture['base']:08X}`", source)
                self.assertEqual(bytes_from_table(f"rv32i-program{number}-initial-data"), fixture["initialData"])
                self.assertEqual(bytes_from_table(f"rv32i-program{number}-stores"), fixture["expected"]["writes"])
                self.assertEqual(fixture["writes"], fixture["expected"]["writes"])
                expected = {f"x{reg}": value for reg, value in fixture["expected"]["registers"].items()}
                for label, key in (("PC", "pc"), ("Attempts", "count"), ("HALTED", "halted"),
                                   ("TRAPPED", "trapped"), ("TRAP_CAUSE", "cause")):
                    expected[label] = fixture["expected"][key]
                    self.assertEqual(fixture[key], expected[label])
                documented = {key: int(value, 0) for key, value, _ in table(f"rv32i-program{number}-result")}
                self.assertEqual(documented, expected)
                for reg, value in fixture["expected"]["registers"].items():
                    self.assertEqual(fixture["registers"][int(reg)], value)

    def test_full_program_assembly_matches_loaded_words(self):
        encodings = {name.lower(): (fmt, int(match, 16))
                     for name, fmt, _, match in table("rv32i-encodings")}

        def reg(value):
            self.assertRegex(value, r"^x(?:[0-9]|[12][0-9]|3[01])$")
            return int(value[1:])

        def immediate(value, width):
            self.assertTrue(-(1 << (width - 1)) <= value < (1 << (width - 1)))
            return value & ((1 << width) - 1)

        for number, fixture in program_fixtures().items():
            source = PROGRAM_DOCUMENTS[number]
            code = re.search(rf"<!-- rv32i-program{number}-assembly -->\n```asm\n(.*?)\n```", source, re.S)[1]
            labels, rows = {}, []
            pc = fixture["base"]
            for line in code.splitlines():
                if line.endswith(":"):
                    self.assertNotIn(line[:-1], labels)
                    labels[line[:-1]] = pc
                    continue
                match = re.fullmatch(r"(.*?)\s+# (0x[0-9A-F]+) \| (0x[0-9A-F]+)", line)
                self.assertIsNotNone(match, (number, line))
                assembly, address, word = match.groups()
                self.assertEqual(int(address, 16), pc)
                rows.append((pc, assembly.strip(), int(word, 16)))
                pc += 4
            self.assertEqual([row[2] for row in rows], fixture["words"], number)

            # Encode the forms used by these listings, independently of the
            # fixture's C++ encoders. This is a test helper, not a new assembler.
            for pc, assembly, word in rows:
                with self.subTest(program=number, pc=hex(pc), assembly=assembly):
                    op, *rest = assembly.split(maxsplit=1)
                    args = [s.strip() for s in rest[0].split(",")] if rest else []
                    if op == ".word":
                        self.assertEqual(int(args[0], 0), word)
                        continue
                    fmt, encoded = encodings[op]
                    if op in ("ecall", "ebreak"):
                        self.assertEqual(args, [])
                    elif op == "fence":
                        self.assertEqual(args, ["0", "0"])
                    elif fmt == "R":
                        encoded |= reg(args[0]) << 7 | reg(args[1]) << 15 | reg(args[2]) << 20
                    elif fmt == "U":
                        value = int(args[1], 0)
                        self.assertTrue(0 <= value < 1 << 20)
                        encoded |= reg(args[0]) << 7 | value << 12
                    elif fmt == "B":
                        offset = immediate(labels[args[2]] - pc, 13)
                        self.assertEqual(offset & 1, 0)
                        encoded |= reg(args[0]) << 15 | reg(args[1]) << 20
                        encoded |= (offset >> 12) << 31 | ((offset >> 11) & 1) << 7
                        encoded |= ((offset >> 5) & 63) << 25 | ((offset >> 1) & 15) << 8
                    elif fmt == "J":
                        offset = immediate(labels[args[1]] - pc, 21)
                        self.assertEqual(offset & 1, 0)
                        encoded |= reg(args[0]) << 7 | (offset >> 20) << 31
                        encoded |= ((offset >> 12) & 255) << 12 | ((offset >> 11) & 1) << 20
                        encoded |= ((offset >> 1) & 1023) << 21
                    elif fmt == "S" or op in ("lb", "lh", "lw", "lbu", "lhu", "jalr"):
                        value, base = re.fullmatch(r"(-?\d+)\((x\d+)\)", args[1]).groups()
                        offset = immediate(int(value), 12)
                        encoded |= reg(base) << 15
                        if fmt == "S":
                            encoded |= reg(args[0]) << 20 | (offset >> 5) << 25 | (offset & 31) << 7
                        else:
                            encoded |= reg(args[0]) << 7 | offset << 20
                    else:
                        self.assertEqual(fmt, "I")
                        offset = int(args[2], 0)
                        if op in ("slli", "srli", "srai"):
                            self.assertTrue(0 <= offset < 32)
                        else:
                            offset = immediate(offset, 12)
                        encoded |= reg(args[0]) << 7 | reg(args[1]) << 15 | offset << 20
                    self.assertEqual(encoded, word)

    def test_explained_array_sum_iterations(self):
        steps = program_fixtures()[1]["steps"]
        for checkpoint, loaded, x1, x2, x3, next_pc in table("rv32i-program1-loop"):
            trace = steps[int(checkpoint[1:])]
            self.assertEqual(trace["pc"], 0x1c)
            self.assertEqual(trace["nextPC"], int(next_pc, 0))
            self.assertEqual([trace["registers"][i] for i in (4, 1, 2, 3)],
                             [int(v, 0) for v in (loaded, x1, x2, x3)])


if __name__ == "__main__":
    unittest.main()
