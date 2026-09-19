"""Native export/schema checks for only the newly documented memory components."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
COMPILER = sys.argv.pop(1) if len(sys.argv) > 1 else "c++"
sys.path.insert(0, str(ROOT / "tools"))
from mkdocs_examples import render_table


class MemoryDeviceDocumentationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {section: (ROOT / "site-docs/memory" / f"{section}.md").read_text()
                       for section in ("3.1.0", "3.1.3", "3.1.4", "3.1.5")}
        cls.components = []
        cls.topologies = {}
        for source in cls.sources.values():
            for section in re.split(r"(?m)^## (?=(?:[\d.]+ )?`)", source)[1:]:
                scenario, test = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", section)[0]
                cls.components.append((scenario, test, section))
                cls.topologies[scenario] = json.loads(subprocess.check_output(
                    [PROBE, "topology", test, scenario], text=True))

    def test_component_documents(self):
        count = 0
        for source in self.sources.values():
            intro = re.split(r"(?m)^## ", source, maxsplit=1)[0]
            table = re.search(r"\| Component \| Description \| Structural \| Behavioral \|\n"
                              r"\| --- \| --- \| :---: \| :---: \|\n((?:\|.*\|\n)+)", intro)
            self.assertIsNotNone(table)
            components = re.findall(r"(?m)^\| `([^`]+)` \| [^|\n]+ \| (Yes|No) \| (Yes|No) \|$", table[1])
            markers = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", source)
            self.assertEqual(len(components), len(markers))
            self.assertEqual(len(markers), source.count('class="simulator-embed"'))
            self.assertIn("Structural", intro)
            for (name, structural, behavioral), (scenario, test) in zip(components, markers):
                def probe(operation):
                    return json.loads(subprocess.check_output([PROBE, operation, test, scenario], text=True))
                topology, examples = self.topologies[scenario], probe("examples")
                root = next(c for c in topology["components"] if c["id"] == topology["rootId"])
                self.assertEqual(root["type"].split("<")[0], name.split("<")[0])
                self.assertEqual(set(root["availableFidelities"]),
                                 {f for f, v in (("structural", structural), ("behavioral", behavioral)) if v == "Yes"})
                self.assertEqual(len(examples["rows"]), len(topology["checkpoints"]))
                html = render_table(examples, scenario, test)
                self.assertIn('class="example-table-scroll"', html)
                self.assertIn('tabindex="0"', html)
                for row, checkpoint in zip(examples["rows"], topology["checkpoints"]):
                    self.assertEqual((row["checkpoint"], row["time"], row["timelineIndex"]),
                                     (checkpoint["label"], checkpoint["time"], checkpoint["index"]))
                count += 1
        self.assertEqual(count, 21)

    def test_established_component_pattern_and_exact_pins(self):
        self.assertEqual(len(self.components), 21)
        for scenario, test, section in self.components:
            with self.subTest(scenario=scenario):
                self.assertEqual(re.findall(r"(?m)^### (.+)$", section),
                                 ["IO pins", "Behavior", "Implementation", "Examples", "Source and tests"])
                behavior = section.split("### Behavior\n")[1].split("### Implementation\n")[0]
                self.assertEqual(re.findall(r"(?m)^#### (.+)$", behavior)[-2:],
                                 ["C++ example", "Unknown inputs and timing"])
                self.assertEqual(section.count("```cpp\n"), 1)
                self.assertIn("#### Embedded simulation\n", section)
                self.assertIn(f'href="../../simulator/?scenario={scenario}" class="md-button"', section)
                topology = self.topologies[scenario]
                root = next(c for c in topology["components"] if c["id"] == topology["rootId"])
                implementation = section.split("### Implementation\n")[1].split("### Examples\n")[0]
                expected = ["Structural implementation"]
                if "behavioral" in root["availableFidelities"]:
                    expected.append("Behavioral implementation")
                self.assertEqual(re.findall(r"(?m)^#### (.+)$", implementation), expected)
                pins = {p["name"]: p for p in topology["pins"] if p["componentId"] == topology["rootId"]}
                documented = set()
                io = section.split("### IO pins\n")[1].split("### Behavior\n")[0]
                for names, direction, width in re.findall(r"(?m)^\| ([^|]+) \| (Input|Output) \| ([^|]+) \|", io):
                    width = width.replace("each", "").strip()
                    size = {"W": 8, "W/8": 1}.get(width)
                    if size is None:
                        size = int(width)
                    for name in re.findall(r"`([^`]+)`", names):
                        pattern = re.sub(r"_[cr]$", r"_\\d+", name)
                        matched = [pin for pin in pins if re.fullmatch(pattern, pin)]
                        self.assertTrue(matched, (scenario, name))
                        for pin in matched:
                            self.assertNotIn(pin, documented)
                            documented.add(pin)
                            self.assertEqual((pins[pin]["type"], pins[pin]["width"]), (direction.lower(), size))
                self.assertEqual(documented, set(pins), scenario)

    def test_cpp_examples_compile_and_execute(self):
        snippets = [re.findall(r"```cpp\n(.*?)\n```", section, re.S)[0]
                    for _, _, section in self.components]
        # Instantiate compositional examples too, rather than merely parsing
        # templates whose bodies could otherwise remain unchecked.
        harness = (ROOT / "tests/fixtures/memory/device-reference-examples.cpp").read_text()
        checks = ["int main() { check_device_references();"]
        count = 0
        for scenario, test, flash in (
                ("dram", "DRAMTest", False),
                ("dram-command-control", "DRAMCommandControlTest", False),
                ("nor-flash", "NORFlashTest", True),
                ("nor-flash-command-control", "NORFlashCommandControlTest", True)):
            rows = json.loads(subprocess.check_output([PROBE, "examples", test, scenario], text=True))["rows"]
            kind = "Flash" if flash else "DRAM"
            config = "4,2,2,32,128,256" if flash else "4,2,32,1000000000"
            checks.append(f"{{ {kind}Reference<8> d({config}); {kind}ControlReference<8>::Inputs in; std::size_t t=0;")
            fields = {"BUSY": "busy", "ERROR": "error",
                      "READ_VALID": "valid" if flash else "output",
                      "OUTPUT_ENABLE": "valid" if flash else "output",
                      "ROW_OPEN": "open", "WRITE_ENABLED": "wel"}
            for row in rows:
                pins = ("CLK", "CMD", "WORD_ADDR" if flash else "ADDR", "WRITE_DATA", "WRITE_MASK")
                inputs = row["inputs"]
                if not all(set(inputs[p]) <= {"0", "1"} for p in pins):
                    break  # Binary prefix, never discard a state-changing unknown input.
                values = [str(int(inputs[p], 2)) + ("u" if i else "") for i, p in enumerate(pins)]
                checks.append("in = {" + ",".join(values) + "}; d.update(++t,in); drain(d,in,t);")
                for pin, value in row["expectedOutputs"].items():
                    if pin == "READ_DATA":
                        checks.append(f'assert(word_text(d.read_data,d.high_z) == "{value}");')
                    else:
                        self.assertIn(pin, fields)
                        checks.append(f"assert(d.control.{fields[pin]} == {int(value, 2)});")
                    count += 1
            checks.append("}")
        self.assertGreaterEqual(count, 60)
        harness += "\n" + "\n".join(checks) + "\n}\n"
        with tempfile.TemporaryDirectory(prefix="memory-doc-cpp-") as tmp:
            binary = Path(tmp) / "examples"
            subprocess.run([COMPILER, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
                            "-x", "c++", "-", "-o", str(binary)],
                           input="\n".join(snippets) + harness, text=True, check=True, timeout=60)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_navigation_and_renumbering(self):
        config = (ROOT / "mkdocs.yml").read_text()
        self.assertIn("          - memory/3.1/index.md", config.splitlines())
        self.assertNotIn("Architecture: memory/", config)
        self.assertFalse((ROOT / "site-docs/memory/3.1.md").exists())
        overview = (ROOT / "site-docs/memory/3.1/index.md").read_text()
        self.assertIn("../3.1.0.md", overview)
        self.assertNotIn("<!-- simulator-examples:", overview)
        self.assertNotIn("### IO pins", overview)
        self.assertNotIn("```cpp", overview)
        for section, title in (("3.1.0", "Helper Components"),
                               ("3.1.1", "Flip-Flop Storage"), ("3.1.2", "SRAM"),
                               ("3.1.3", "DRAM"), ("3.1.4", "ROM"), ("3.1.5", "NOR Flash"),
                               ("3.1.6", "Scalable Memory Container"),
                               ("3.2", "Memory Transactions"), ("3.3", "Memory Controllers")):
            self.assertIn(f"memory/{section}.md", config)
            source = (ROOT / f"site-docs/memory/{section}.md").read_text()
            self.assertTrue(source.startswith(f"# § {section}. {title}"))
            # Devices belong under 3.1; access and control are sibling sections.
            indent = 10 if section.startswith("3.1.") else 6
            self.assertIn(f"{' ' * indent}- {section}. {title}: memory/{section}.md", config.splitlines())
        for retired in ("3.1.7", "3.1.8"):
            self.assertNotIn(f"memory/{retired}.md", config)
            self.assertFalse((ROOT / f"site-docs/memory/{retired}.md").exists())


if __name__ == "__main__":
    unittest.main()
