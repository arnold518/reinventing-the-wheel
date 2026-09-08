"""Native example-export contracts and dependency-free table renderer tests."""

from copy import deepcopy
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest

REPOSITORY = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY / "tools"))
from mkdocs_examples import MARKER, format_value, render_table

PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else None


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
        for path in sorted((REPOSITORY / "site-docs/alu8").glob("*.md")):
            source = path.read_text()
            markers = MARKER.findall(source)
            cls.sources[path] = source
            for scenario, test in markers:
                cls.exports[scenario] = json.loads(subprocess.check_output(
                    [PROBE, "examples", test, scenario], text=True))

    def test_complete_scenario_coverage(self):
        expected_counts = {"adder8": 14, "addsub8": 80, "arithmetic8": 80, "logic8": 32,
            "barrel-shifter8": 96, "zero-detect8": 11, "compare-select8": 40,
            "alu8-flags": 128, "alu8-control": 32, "alu8-result-selector": 66, "alu8": 70}
        self.assertEqual({key: len(data["rows"]) for key, data in self.exports.items()}, expected_counts)
        self.assertEqual(sum(expected_counts.values()), 649)
        for path, source in self.sources.items():
            self.assertEqual(len(MARKER.findall(source)), source.count("### Examples"), path)
            self.assertEqual(len(MARKER.findall(source)), source.count('class="simulator-embed"'), path)
            for section in re.findall(r"### Examples\n(.*?)#### Embedded simulation", source, re.S):
                self.assertNotRegex(section, r"(?m)^\|") # No duplicated hand-maintained table.

    def test_component_description_tables(self):
        for path, source in self.sources.items():
            with self.subTest(page=path.name):
                intro = re.split(r"(?m)^## ", source, maxsplit=1)[0]
                table = re.search(
                    r"(?m)^\| Component \| Description \|\n\| --- \| --- \|\n((?:\|.*\|\n)+)",
                    intro)
                self.assertIsNotNone(table, "Each component document needs an overview table")
                components = re.findall(r"(?m)^\| `([^`]+)` \| [^\n]+ \|$", table.group(1))
                sections = re.findall(r"(?m)^## 2\.\d+\.\d+\. `([^`]+)`$", source)
                self.assertEqual(components, sections)
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
        for field, value in [("time", -1), ("rowIndex", 8), ("checkpoint", "row-8"), ("inputs", {})]:
            broken = deepcopy(base)
            broken["rows"][0][field] = value
            with self.assertRaises(ValueError):
                render_table(broken, "alu8", "ALU8Test")

    def test_unasserted_output_is_not_invented(self):
        data = deepcopy(self.exports["alu8"])
        del data["rows"][0]["expectedOutputs"]["OUT"]
        self.assertIn('not checked', render_table(data, "alu8", "ALU8Test"))


if __name__ == "__main__":
    unittest.main()
