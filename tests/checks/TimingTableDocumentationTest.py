"""Delay tables in the documentation come from the stored proven tables.

Every stored table must render with the same paths in every timing profile,
and every `<!-- timing-table: KEY -->` marker in site-docs must name a stored
table. The hook reads the generated table files directly, so the rendered
numbers are the ones the simulator uses.
"""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mkdocs_timing  # noqa: E402


class TimingTables(unittest.TestCase):
    def test_every_stored_table_renders_in_every_profile(self):
        tables = mkdocs_timing.profiles()
        names = set(tables["unit"][0])
        self.assertGreater(len(names), 100)
        for profile, (stored, _) in tables.items():
            self.assertEqual(set(stored), names, profile)
        for key in sorted(names):
            with self.subTest(key=key):
                self.assertIn("<table", mkdocs_timing.render(key))

    def test_known_values(self):
        # The 8-bit adder: A -> Cout, as stored for each profile.
        html = mkdocs_timing.render("arithmetic.adder.width8.behavioral.direct")
        row = re.search(r"A → Cout</th>(.*?)</tr>", html).group(1)
        self.assertEqual(re.findall(r"<td>(.*?)</td>", row), ["3–18", "102–936", "71–652"])
        # A clocked part shows clock-to-output, setup and its internal period.
        html = mkdocs_timing.render("storage.register.32.behavioral.word")
        for label in ("CLK ↑ → Q", "D before CLK ↑", "Shortest internal clock period"):
            self.assertIn(label, html)

    def test_markers_name_stored_tables(self):
        for page in sorted((ROOT / "site-docs").rglob("*.md")):
            for key in mkdocs_timing.MARKER.findall(page.read_text(encoding="utf-8")):
                with self.subTest(page=str(page.relative_to(ROOT)), key=key):
                    mkdocs_timing.render(key)

    def test_unknown_table_is_refused(self):
        with self.assertRaises(ValueError):
            mkdocs_timing.render("no.such.part.behavioral")


if __name__ == "__main__":
    unittest.main()
