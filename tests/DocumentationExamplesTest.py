"""Native example-export contracts and dependency-free table renderer tests."""

from copy import deepcopy
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY / "tools"))
from mkdocs_examples import MARKER, format_value, render_table

PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else None
COMPILER = sys.argv.pop(1) if len(sys.argv) > 1 else "c++"


class TableParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.rows = []
        self.regions = []
        self.cells = []

    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        if tag == "tr" and "data-checkpoint" in attrs:
            self.rows.append(attrs)
        if tag == "td" and "data-pin" in attrs:
            self.cells.append(attrs)
        if attrs.get("class") == "example-table-scroll":
            self.regions.append(attrs)


class DocumentationExamplesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PROBE:
            raise RuntimeError("Pass the native VisualizerSessionProbe path")
        cls.exports = {}
        cls.sources = {}
        cls.topologies = {}
        cls.states = {}
        # Keep this legacy ALU/FF/SRAM suite bounded. New technologies have their
        # own MemoryDeviceDocumentationTest and sequential browser checks.
        paths = [*(REPOSITORY / "site-docs/alu8").glob("*.md"),
                 REPOSITORY / "site-docs/memory/3.1.1.md",
                 REPOSITORY / "site-docs/memory/3.1.2.md"]
        for path in sorted(paths):
            source = path.read_text()
            markers = MARKER.findall(source)
            cls.sources[path] = source
            for scenario, test in markers:
                cls.exports[scenario] = json.loads(subprocess.check_output(
                    [PROBE, "examples", test, scenario], text=True))

    def topology(self, scenario):
        if scenario not in self.topologies:
            self.topologies[scenario] = json.loads(subprocess.check_output(
                [PROBE, "topology", self.exports[scenario]["registeredTest"], scenario], text=True))
        return self.topologies[scenario]

    def state(self, scenario, index):
        key = scenario, index
        if key not in self.states:
            self.states[key] = json.loads(subprocess.check_output(
                [PROBE, "state", self.exports[scenario]["registeredTest"], scenario, str(index)], text=True))
        return self.states[key]

    def test_complete_scenario_coverage(self):
        expected_counts = {"half-adder": 16, "full-adder": 64,
            "adder8": 14, "addsub8": 80, "arithmetic8": 80, "logic8": 32,
            "barrel-shifter8": 96, "zero-detect8": 11, "compare-select8": 40,
            "alu8-flags": 128, "alu8-control": 32, "alu8-result-selector": 66, "alu8": 70,
            "sr-latch": 8, "gated-d-latch": 12, "dff": 14, "flip-flop-cell": 13,
            "ff-word": 30, "ff-storage-bank": 134, "ff-memory-container": 52,
            "sram-cell": 9, "sram-row": 5, "sram-bitline-pair": 13, "sram-storage-bank": 16,
            "sram-row-decoder": 9, "sram-precharge": 4, "sram-write-driver": 8,
            "sram-sense-amplifier": 5, "sram-column-selector": 2, "sram-access-control": 5,
            "sram-output-buffer": 4, "sram": 35}
        self.assertEqual({key: len(data["rows"]) for key, data in self.exports.items()}, expected_counts)
        self.assertEqual(sum(expected_counts.values()), 1107)
        for path, source in self.sources.items():
            count = len(MARKER.findall(source))
            self.assertEqual(count, source.count("### Examples"), path)
            self.assertEqual(count, source.count('class="simulator-embed"'), path)
            self.assertNotIn("simulator-observation:", source)
            for section in re.findall(r"### Examples\n(.*?)#### Embedded simulation", source, re.S):
                self.assertNotRegex(section, r"(?m)^\|") # No duplicated hand-maintained table.

    def test_component_description_tables(self):
        for path, source in self.sources.items():
            with self.subTest(page=path.name):
                intro = re.split(r"(?m)^## ", source, maxsplit=1)[0]
                table = re.search(
                    r"(?m)^\| Component \| Description \| Structural \| Behavioral \|\n"
                    r"\| --- \| --- \| :---: \| :---: \|\n((?:\|.*\|\n)+)",
                    intro)
                self.assertIsNotNone(table, "Each component document needs an overview table")
                rows = re.findall(r"(?m)^\| `([^`]+)` \| ([^|\n]+) \| (Yes|No) \| (Yes|No) \|$", table.group(1))
                self.assertEqual(len(rows), len(table.group(1).splitlines()))
                components = [row[0] for row in rows]
                names = re.findall(r"(?m)^## [23]\.(?:\d+\.){2,3} `([^`]+)`$", source)
                self.assertEqual(components, names)
                sections = re.split(r"(?m)^## [23]\.(?:\d+\.){2,3}", source)[1:]
                for (component, _, structural, behavioral), section in zip(rows, sections, strict=True):
                    markers = MARKER.findall(section)
                    self.assertEqual(len(markers), 1)
                    scenario, test = markers[0]
                    self.assertEqual(self.exports[scenario]["registeredTest"], test)
                    topology = self.topology(scenario)
                    component_id = topology["rootId"]
                    actual = next(item for item in topology["components"] if item["id"] == component_id)
                    generic_types = {"FFWord<W>": r"FFWord<(8|16|32)>",
                                     "FFStorageBank<W>": r"FFStorageBank<(8|16|32),[1-9][0-9]*>",
                                     "FFMemory<W>": r"FFMemory(8|16|32)",
                                     "SRAMRow": r"SRAMRow<[1-9][0-9]*>",
                                     "SRAMBitlinePair": r"SRAMBitlinePair<[1-9][0-9]*>",
                                     "SRAMPrecharge": r"SRAMPrecharge<[1-9][0-9]*>",
                                     "SRAMRowDecoder": r"SRAMRowDecoder<[1-9][0-9]*,[1-9][0-9]*>",
                                     "SRAMStorageBank<W>": r"SRAMStorageBank<(8|16|32),[1-9][0-9]*,[1-9][0-9]*>",
                                     "SRAMColumnSelector<W>": r"SRAMColumnSelector<(8|16|32),[1-9][0-9]*>",
                                     "SRAMAccessControl<W>": r"SRAMAccessControl<(8|16|32)>",
                                     "SRAMOutputBuffer<W>": r"SRAMOutputBuffer<(8|16|32)>",
                                     "SRAM<W>": r"SRAM<(8|16|32)>"}
                    self.assertRegex(actual["type"], "^" + generic_types.get(component, re.escape(component)) + "$")
                    documented = {name for name, exists in (("structural", structural), ("behavioral", behavioral))
                                  if exists == "Yes"}
                    self.assertEqual(documented, set(actual["availableFidelities"]), component)
                self.assertNotIn("Replacement ALU8", source)
                self.assertNotIn("ALU8V2", source)

    def test_rendered_values_and_accessibility(self):
        for scenario, data in self.exports.items():
            parser = TableParser()
            parser.feed(render_table(data, scenario, data["registeredTest"]))
            self.assertEqual(len(parser.rows), len(data["rows"]))
            self.assertEqual(parser.regions[0]["tabindex"], "0")
            self.assertEqual(parser.regions[0]["role"], "region")
            columns = len(data["inputs"]) + len(data["outputs"])
            self.assertEqual(len(parser.cells), len(data["rows"]) * columns)
            for index, row in enumerate(data["rows"]):
                self.assertEqual(parser.rows[index]["data-checkpoint"], row["checkpoint"])
                self.assertEqual(int(parser.rows[index]["data-time"]), row["time"])
                for cell in parser.cells[index * columns:(index + 1) * columns]:
                    self.assertEqual(cell["data-bits"], row[cell["data-direction"]].get(cell["data-pin"], ""))

    def test_formats(self):
        self.assertIn('>0x01<', format_value("00000001", 8))
        self.assertIn('>1<', format_value("1", 1))
        self.assertIn('>0b1000X00Z<', format_value("1000X00Z", 8))
        self.assertIn('not checked', format_value(None, 8))
        for value, width in [("<script>", 8), ("10", 1), ("G", 1), (1, 1)]:
            with self.assertRaises(ValueError):
                format_value(value, width)

    def test_reject_inconsistent_metadata(self):
        base = self.exports["alu8"]
        for field, value in [("registeredTest", "WrongTest"), ("schemaVersion", 99), ("rows", [])]:
            broken = deepcopy(base)
            broken[field] = value
            with self.assertRaises(ValueError):
                render_table(broken, "alu8", "ALU8Test")
        for field, value in [("time", -1), ("rowIndex", 8), ("checkpoint", ""),
                             ("checkpoint", "row-8"), ("inputs", {})]:
            broken = deepcopy(base)
            broken["rows"][0][field] = value
            with self.assertRaises(ValueError):
                render_table(broken, "alu8", "ALU8Test")

    def test_unasserted_output_is_not_invented(self):
        data = deepcopy(self.exports["alu8"])
        del data["rows"][0]["expectedOutputs"]["OUT"]
        self.assertIn('not checked', render_table(data, "alu8", "ALU8Test"))

    def test_component_documentation_pattern(self):
        expected = ["IO pins", "Behavior", "Implementation", "Examples", "Source and tests"]
        component_count = 0
        for path, source in self.sources.items():
            sections = re.split(r"(?m)^## [23]\.\d+\.\d+\.", source)[1:]
            self.assertEqual(len(sections), len(MARKER.findall(source)), path)
            for section in sections:
                component_count += 1
                component = re.search(r"`([^`]+)`", section.splitlines()[0]).group(1)
                with self.subTest(page=path.name, component=component):
                    self.assertEqual(re.findall(r"(?m)^### (.+)$", section), expected)
                    behavior = section.split("### Behavior\n", 1)[1].split("### Implementation\n", 1)[0]
                    implementation = section.split("### Implementation\n", 1)[1].split("### Examples\n", 1)[0]
                    behavior_headings = re.findall(r"(?m)^#### (.+)$", behavior)
                    implementation_headings = re.findall(r"(?m)^#### (.+)$", implementation)
                    # Operation/flag tables may have their own subheadings.
                    self.assertEqual(behavior_headings[-2:], ["C++ example", "Unknown inputs and timing"])
                    self.assertEqual(behavior_headings.count("C++ example"), 1)
                    self.assertEqual(implementation_headings[0], "Structural implementation")
                    has_behavioral = component not in {"HalfAdder", "FullAdder", "SRLatch", "GatedDLatch", "DFlipFlop",
                        "SRAMBitlinePair", "SRAMRowDecoder", "SRAMPrecharge", "SRAMWriteDriver",
                        "SRAMSenseAmplifier", "SRAMColumnSelector<W>", "SRAMAccessControl<W>", "SRAMOutputBuffer<W>"}
                    self.assertEqual(implementation_headings.count("Behavioral implementation"), int(has_behavioral))
                    if has_behavioral:
                        self.assertEqual(implementation_headings[-1], "Behavioral implementation")
                    else:
                        self.assertEqual(implementation_headings, ["Structural implementation"])
                    example = behavior.split("#### C++ example\n", 1)[1].split("#### Unknown inputs and timing\n", 1)[0]
                    self.assertEqual(example.count("```cpp\n"), 1)
                    self.assertEqual(section.count("```cpp\n"), 1)
                    self.assertNotIn("#### Binary behavioral reference", section)
                    self.assertNotIn("Simulator variants", source)
                    self.assertNotIn("#### Structural circuit", section)
        self.assertEqual(component_count, 32)

    def test_arithmetic_reference_order_and_old_links(self):
        source = self.sources[REPOSITORY / "site-docs/alu8/2.1.md"]
        self.assertEqual(re.findall(r"(?m)^## 2\.1\.(\d+)\. `([^`]+)`$", source), [
            (str(index), component) for index, component in enumerate(
                ("HalfAdder", "FullAdder", "Adder8", "AddSub8", "ArithmeticUnit8"), 1)])
        for anchor, number, component in (("211-adder8", 3, "Adder8"),
                                           ("212-addsub8", 4, "AddSub8"),
                                           ("213-arithmeticunit8", 5, "ArithmeticUnit8")):
            self.assertEqual(source.count(f'<a id="{anchor}"></a>'), 1)
            self.assertIn(f'<a id="{anchor}"></a>\n\n## 2.1.{number}. `{component}`', source)

    def test_sram_cpp_examples_compile_and_match_binary_device_rows(self):
        source = (REPOSITORY / "site-docs/memory/3.1.2.md").read_text()
        snippets = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
        self.assertEqual(len(snippets), 12)
        checks = []
        for row in self.exports["sram"]["rows"]:
            inputs = row["inputs"]
            pins = ("WORD_ADDR", "WRITE_DATA", "WRITE_MASK", "CE_N", "WE_N", "OE_N")
            # Stop at the first four-state input; never skip a state-changing
            # stimulus and then claim later checkpoints still share history.
            if not all(set(inputs[p]) <= {"0", "1"} for p in pins):
                break
            values = [str(int(inputs[p], 2)) + ("u" if i < 3 else "") for i, p in enumerate(pins)]
            checks.append("in = {" + ",".join(values) + "}; device.update(++time, 32, in);")
            checks.append("while (device.control.due) { time = device.control.due; device.update(time, 32, in); }")
            result = row["expectedOutputs"].get("READ_DATA")
            if result is not None:
                checks.append(f'assert(word_text(device.read_data, device.high_z) == "{result}");')
        self.assertGreaterEqual(sum(line.startswith("assert") for line in checks), 15)
        harness = (REPOSITORY / "tests/fixtures/memory/sram-reference-examples.cpp").read_text()
        program = "\n".join(snippets) + harness + '''
int main() {
    check_sram_references();
    SRAMReference<8> device(4); SRAMControlReference::Inputs in; std::size_t time = 0;
''' + "\n".join(checks) + "\n}\n"
        with tempfile.TemporaryDirectory(prefix="circuitsim-sram-examples-") as directory:
            path = Path(directory) / "reference.cpp"
            executable = Path(directory) / "reference"
            path.write_text(program)
            subprocess.run([COMPILER, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_alu8_cpp_examples_compile_and_match_recorded_rows(self):
        snippets = []
        for path, source in self.sources.items():
            if path.parent.name == "alu8":
                snippets.extend(re.findall(r"```cpp\n(.*?)\n```", source, re.S))
        self.assertEqual(len(snippets), 13)
        calls = {
            "half-adder": ("half_adder_ref", "A B"),
            "full-adder": ("full_adder_ref", "A B Carry_in"),
            "adder8": ("adder8_ref", "A B Cin"),
            "addsub8": ("addsub8_ref", "A B SUB C_IN"),
            "arithmetic8": ("arithmetic8_ref", "A B OP C_IN"),
            "logic8": ("logic8_ref", "A B OP"),
            "barrel-shifter8": ("shift8_ref", "A B OP C_IN"),
            "zero-detect8": ("zero_detect8_ref", "A"),
            "compare-select8": ("compare_select8_ref", "A B DIFF CARRY_IN OVERFLOW_IN OP"),
            "alu8-flags": ("flags8_ref", "RESULT ARITHMETIC SHIFT ARITH_CARRY ARITH_OVERFLOW SHIFT_CARRY"),
            "alu8-control": ("alu8_control_ref", "OP"),
            "alu8-result-selector": ("alu8_result_selector_ref", ""),
            "alu8": ("alu8_ref", "A B OP C_IN"),
        }
        scalar_results = {"logic8", "zero-detect8", "compare-select8", "alu8-result-selector"}
        checks = []
        row_counts = {}
        for scenario, (function, parameters) in calls.items():
            row_counts[scenario] = 0
            for row in self.exports[scenario]["rows"]:
                # These examples intentionally cannot represent X/Z inputs.
                if any(set(bits) - {"0", "1"} for bits in row["inputs"].values()):
                    continue
                inputs = {name: str(int(bits, 2)) for name, bits in row["inputs"].items()}
                arguments = ", ".join(inputs[name] for name in parameters.split())
                if scenario == "alu8-result-selector":
                    sources = "ARITHMETIC_RESULT LOGIC_RESULT SHIFT_RESULT COMPARE_RESULT A B".split()
                    enables = "ARITHMETIC_ENABLE LOGIC_ENABLE SHIFT_ENABLE COMPARE_ENABLE PASS_A_ENABLE PASS_B_ENABLE".split()
                    arguments = ("{" + ", ".join(inputs[name] for name in sources) + "}, {" +
                                 ", ".join(inputs[name] for name in enables) + "}")
                checks.append(f"{{ // {scenario}: {row['checkpoint']}\n"
                              f"const auto result = {function}({arguments});")
                for name, bits in row["expectedOutputs"].items():
                    result = "result" if scenario in scalar_results else f"result.{name}"
                    checks.append(f"assert({result} == {int(bits, 2)});")
                checks.append("}")
                row_counts[scenario] += 1
        self.assertEqual(row_counts, {
            "half-adder": 4, "full-adder": 8,
            "adder8": 12, "addsub8": 80, "arithmetic8": 80, "logic8": 32,
            "barrel-shifter8": 96, "zero-detect8": 7, "compare-select8": 40,
            "alu8-flags": 128, "alu8-control": 32, "alu8-result-selector": 64, "alu8": 70})
        program = "\n".join(snippets) + "\n#include <cassert>\nint main() {\n" + "\n".join(checks) + "\n}\n"
        with tempfile.TemporaryDirectory(prefix="circuitsim-alu8-examples-") as directory:
            source_path = Path(directory) / "examples.cpp"
            executable = Path(directory) / "examples"
            source_path.write_text(program)
            subprocess.run([COMPILER, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(source_path), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_storage_checkpoint_history(self):
        for scenario in ("sr-latch", "gated-d-latch", "dff", "flip-flop-cell", "ff-word", "ff-storage-bank", "ff-memory-container"):
            data = self.exports[scenario]
            topology = self.topology(scenario)
            pins = {p["name"]: p["stateIndex"] for p in topology["pins"]
                    if p["componentId"] == topology["rootId"]}
            for row, checkpoint in zip(data["rows"], topology["checkpoints"], strict=True):
                self.assertEqual([row["rowIndex"], row["checkpoint"], row["time"], row["timelineIndex"]],
                                 [checkpoint["rowIndex"], checkpoint["label"], checkpoint["time"], checkpoint["index"]])
                state = self.state(scenario, row["timelineIndex"])
                for name, value in (row["inputs"] | row["expectedOutputs"]).items():
                    self.assertEqual(state["pins"][pins[name]], value, (scenario, row["checkpoint"], name))
        cell = self.exports["flip-flop-cell"]
        self.assertEqual(cell["rootType"], "FlipFlopCell")
        write = next(row for row in cell["rows"] if row["checkpoint"] == "write-one")
        self.assertEqual(write["inputs"], {"D": "1", "WE": "1", "CLK": "1", "RST": "0"})
        self.assertEqual(write["drivenInputs"], {"CLK": "1"})
        self.assertGreater(write["actionIndex"], write["rowIndex"])
        self.assertEqual(write["kind"], "after-edge")
        behavioral = json.loads(subprocess.check_output(
            [PROBE, "examples", "FlipFlopCellTest", "flip-flop-cell",
             "--override", "FLIP_FLOP_CELL_ROOT", "behavioral"], text=True))
        self.assertEqual(behavioral["fidelity"], "behavioral")
        self.assertNotEqual(cell["profileFingerprint"], behavioral["profileFingerprint"])
        for structural_row, behavioral_row in zip(cell["rows"], behavioral["rows"], strict=True):
            for field in ("checkpoint", "inputs", "expectedOutputs", "actionIndex", "drivenInputs"):
                self.assertEqual(structural_row[field], behavioral_row[field])

    def test_storage_binary_references_compile_and_run(self):
        source = self.sources[REPOSITORY / "site-docs/memory/3.1.1.md"]
        snippets = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
        self.assertEqual(len(snippets), 7)
        program = "\n".join(snippets) + r'''
#include <cassert>
int main() {
    SRLatchReference sr;
    sr.update(false, true); assert(sr.q && !sr.q_bar);
    sr.update(true, true); assert(sr.q && !sr.q_bar);
    sr.update(false, false); assert(sr.q && sr.q_bar);
    bool rejected = false;
    try { sr.update(true, true); } catch (const std::logic_error&) { rejected = true; }
    assert(rejected);
    sr.update(true, false); assert(!sr.q && sr.q_bar);
    for (bool old : {false, true}) for (bool d : {false, true}) {
        GatedDLatchReference latch;
        latch.update(old, true, false);
        latch.update(d, false, false); assert(latch.q == old);
        latch.update(d, true, false); assert(latch.q == d && latch.q_bar() != d);
        latch.update(d, true, true); assert(!latch.q);
        DFlipFlopReference ff;
        ff.update(old, false, false); ff.update(old, true, false);
        ff.update(d, true, false); assert(ff.q == old);
        ff.update(d, false, false); assert(ff.q == old);
        ff.update(d, true, false); assert(ff.q == d && ff.q_bar() != d);
        ff.update(d, true, true); assert(!ff.q);
        for (bool we : {false, true}) {
            FlipFlopCellReference cell;
            cell.update(old, true, false, false); cell.update(old, true, true, false);
            cell.update(d, we, true, false); assert(cell.q == old);
            cell.update(d, we, false, false); assert(cell.q == old);
            cell.update(d, we, true, false); assert(cell.q == (we ? d : old));
            cell.update(d, we, true, true); assert(!cell.q);
        }
    }
'''
        # Replay every recorded binary word/device checkpoint through the prose examples.
        checks = ["FFWordReference<16> word;", "FFMemoryReference<8> memory(4);",
                  "FFStorageBankReference<8> bank(4);"]
        for scenario, variable, parameters, outputs in (
                ("ff-word", "word", "WRITE_DATA WRITE_MASK WE CLK RST", {"Q": "q"}),
                ("ff-memory-container", "memory", "WORD_ADDR WRITE_DATA WRITE_MASK WE READ_EN CLK RST",
                 {"READ_DATA": "read_data", "FAULT": "fault"})):
            for row in self.exports[scenario]["rows"]:
                arguments = ", ".join(str(int(row["inputs"][name], 2)) + "u" for name in parameters.split())
                checks.append(f"// {scenario}: {row['checkpoint']}\n{variable}.update({arguments});")
                for pin, field in outputs.items():
                    checks.append(f"assert({variable}.{field} == {int(row['expectedOutputs'][pin], 2)}u);")

        # Replay the standalone bank's full set of word-enable combinations and byte masks.
        checks.append("FFStorageBankReference<16> standalone_bank(3);")
        for row in self.exports["ff-storage-bank"]["rows"]:
            values = {name: int(bits, 2) for name, bits in row["inputs"].items()}
            enables = ", ".join("true" if values[f"WRITE_{n}"] else "false" for n in range(3))
            checks.append(f"// standalone bank: {row['checkpoint']}\nstandalone_bank.update({values['WRITE_DATA']}u, "
                          f"{values['WRITE_MASK']}u, {{{enables}}}, {values['CLK']}, {values['RST']});")
            for n in range(3):
                checks.append(f"assert(standalone_bank.words[{n}].q == {int(row['expectedOutputs'][f'WORD_{n}'], 2)}u);")

        # Also check the bank's internal pins when connected to a complete device.
        scenario = "ff-memory-container"
        topology = self.topology(scenario)
        bank_id = topology["rootId"] + ".STORAGE"
        bank_pins = {p["name"]: p["stateIndex"] for p in topology["pins"] if p["componentId"] == bank_id}
        self.assertEqual(set(bank_pins), {"WRITE_DATA", "WRITE_MASK", "CLK", "RST"}
                         | {f"{prefix}_{n}" for prefix in ("WRITE", "WORD") for n in range(4)})
        observations = {}
        for row in self.exports[scenario]["rows"]:
            state = self.state(scenario, row["timelineIndex"])
            values = {name: int(state["pins"][index], 2) for name, index in bank_pins.items()}
            observations[row["checkpoint"]] = values
            inputs = {name: int(bits, 2) for name, bits in row["inputs"].items()}
            for pin in ("WRITE_DATA", "WRITE_MASK", "CLK", "RST"):
                self.assertEqual(values[pin], inputs[pin], (row["checkpoint"], pin))
            for n in range(4):
                self.assertEqual(values[f"WRITE_{n}"], int(inputs["WE"] and inputs["WORD_ADDR"] == n))
            enables = ", ".join("true" if values[f"WRITE_{n}"] else "false" for n in range(4))
            checks.append(f"// bank: {row['checkpoint']}\nbank.update({values['WRITE_DATA']}u, "
                          f"{values['WRITE_MASK']}u, {{{enables}}}, {values['CLK']}, {values['RST']});")
            for n in range(4):
                checks.append(f"assert(bank.words[{n}].q == {values[f'WORD_{n}']}u);")
        self.assertEqual(observations["write-enable-0"]["WRITE_0"], 1)
        self.assertEqual(observations["write-enable-0"]["WORD_0"], 0)
        self.assertEqual(observations["write-edge-0"]["WORD_0"], 0x81)
        self.assertEqual([observations["read-3"][f"WORD_{n}"] for n in range(4)], [0x81, 0x80, 0x83, 0x82])
        self.assertEqual([observations["reset-retention"][f"WORD_{n}"] for n in range(4)], [0] * 4)
        program += "\n".join(checks) + r'''
    // Wider byte masks, multiple bank enables, non-power-of-two depth and fault/reset independence.
    FFWordReference<32> wide;
    wide.update(0x12345678u, 15, true, true, false);
    wide.update(0xaabbccddu, 5, true, false, false);
    wide.update(0xaabbccddu, 5, true, true, false);
    assert(wide.q == 0x12bb56ddu);
    FFStorageBankReference<16> broadcast(3);
    broadcast.update(0xabcd, 3, {true, false, true}, true, false);
    assert(broadcast.words[0].q == 0xabcd && broadcast.words[1].q == 0 && broadcast.words[2].q == 0xabcd);
    broadcast.update(0, 0, {false, false, false}, true, true);
    for (const auto& entry : broadcast.words) assert(entry.q == 0);
    FFMemoryReference<32> device(3);
    device.update(2, 0x12345678u, 15, true, true, true, false);
    assert(device.read_data == 0x12345678u && !device.fault);
    device.update(3, 0, 0, true, false, false, true);
    assert(device.read_data == 0 && device.fault);
    for (const auto& entry : device.storage.words) assert(entry.q == 0);
    device.update(0xffffffffu, 0, 0, false, false, false, false);
    assert(device.read_data == 0 && !device.fault);
}
'''
        with tempfile.TemporaryDirectory(prefix="circuitsim-storage-reference-") as directory:
            source_path = Path(directory) / "reference.cpp"
            executable = Path(directory) / "reference"
            source_path.write_text(program)
            subprocess.run([COMPILER, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(source_path), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
