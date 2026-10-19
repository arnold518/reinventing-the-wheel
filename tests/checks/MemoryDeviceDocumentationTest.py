"""Native export/schema checks for only the newly documented memory components."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
# Shared building blocks and three technology pages (FF and SRAM have their own test).
PAGES = ("building-blocks", "technologies/dram", "technologies/rom", "technologies/nor-flash")


def components_of(source):
    """Component sections (headings like `## 3.3.3.1. `DRAMCell``), each up to the next level-2 heading."""
    return [part for part in re.split(r"(?m)^## ", source)[1:] if re.match(r"[\d.]+ `", part)]
PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
COMPILER = sys.argv.pop(1) if len(sys.argv) > 1 else "c++"
sys.path.insert(0, str(ROOT / "tools"))
from mkdocs_examples import render_table


class MemoryDeviceDocumentationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {page: (ROOT / "site-docs/memory" / f"{page}.md").read_text() for page in PAGES}
        cls.components = []
        cls.topologies = {}
        for page, source in cls.sources.items():
            for section in components_of(source):
                scenario, test = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", section)[0]
                cls.components.append((scenario, test, section, page))
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
        for scenario, test, section, page in self.components:
            with self.subTest(scenario=scenario):
                self.assertEqual(re.findall(r"(?m)^### (.+)$", section),
                                 ["IO pins", "Behavior", "Implementation", "Timing", "Examples", "Source and tests"])
                behavior = section.split("### Behavior\n")[1].split("### Implementation\n")[0]
                self.assertEqual(re.findall(r"(?m)^#### (.+)$", behavior)[-2:],
                                 ["C++ example", "Unknown inputs"])
                self.assertEqual(section.count("```cpp\n"), 1)
                self.assertIn("#### Embedded simulation\n", section)
                up = "../" * (page.count("/") + 2)
                self.assertIn(f'href="{up}simulator/?scenario={scenario}" class="md-button"', section)
                timing = section.split("### Timing\n")[1].split("### Examples\n")[0]
                self.assertTrue(timing.strip())
                topology = self.topologies[scenario]
                root = next(c for c in topology["components"] if c["id"] == topology["rootId"])
                implementation = section.split("### Implementation\n")[1].split("### Timing\n")[0]
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
                    for _, _, section, _ in self.components]
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
                # The reference finishes each command before the next; a row
                # taken mid-command (the device's pipelined acceptance) only
                # applies its inputs.
                if row["expectedOutputs"].get("BUSY") == "1":
                    continue
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
        config = (ROOT / "mkdocs.yml").read_text().splitlines()
        pages = (("architecture", "3.1", "Memory Architecture", 6),
                 ("building-blocks", "3.2", "Shared Building Blocks", 6),
                 ("technologies/flip-flop", "3.3.1", "Flip-Flop Storage", 10),
                 ("technologies/sram", "3.3.2", "SRAM", 10),
                 ("technologies/dram", "3.3.3", "DRAM", 10),
                 ("technologies/rom", "3.3.4", "ROM", 10),
                 ("technologies/nor-flash", "3.3.5", "NOR Flash", 10),
                 ("technologies/scalable", "3.3.6", "Scalable Native Devices", 10),
                 ("transactions", "3.4", "Transaction Protocol", 6),
                 ("controllers", "3.5", "Controllers and Endpoints", 6),
                 ("timing", "3.6", "Memory Timing", 6),
                 ("caches", "3.7", "Caches", 6))
        for page, number, title, indent in pages:
            source = (ROOT / f"site-docs/memory/{page}.md").read_text()
            self.assertTrue(source.startswith(f"# § {number}. {title}\n"), page)
            self.assertIn(f"{' ' * indent}- {number}. {title}: memory/{page}.md", config)
        # The technologies are subsections of one section.
        self.assertIn("      - 3.3. Memory Technologies:", config)
        self.assertIn("          - memory/technologies/index.md", config)
        self.assertTrue((ROOT / "site-docs/memory/technologies/index.md").read_text().startswith("# § 3.3. Memory Technologies\n"))
        for retired in ("3.1.md", "3.1/index.md", "3.1.0.md", "3.1.1.md", "3.1.2.md", "3.1.3.md", "3.1.4.md",
                        "3.1.5.md", "3.1.6.md", "3.2.md", "3.3.md", "3.4.md"):
            self.assertFalse((ROOT / f"site-docs/memory/{retired}").exists(), retired)
            self.assertNotIn(f"memory/{retired}", "\n".join(config))
        overview = (ROOT / "site-docs/memory/architecture.md").read_text()
        self.assertNotIn("<!-- simulator-examples:", overview)
        self.assertNotIn("### IO pins", overview)
        self.assertNotIn("```cpp", overview)
        # Timing is stated by the profiles, not as abstract units.
        for path in (ROOT / "site-docs/memory").rglob("*.md"):
            text = path.read_text()
            self.assertNotIn("abstract ticks", text, path)
            self.assertNotIn("Simulator ticks are abstract", text, path)
            self.assertNotIn("Unknown inputs and timing", text, path)
        self.assertIn("<!-- latency-table: all -->", (ROOT / "site-docs/memory/timing.md").read_text())


if __name__ == "__main__":
    unittest.main()
