"""Delay tables in the documentation come from the stored proven tables.

Every stored table must render with the same paths in every timing profile,
and every `<!-- timing-table: KEY -->` marker in site-docs must name a stored
table. The hook reads the generated table files directly, so the rendered
numbers are the ones the simulator uses.
"""
from pathlib import Path
import re
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mkdocs_timing  # noqa: E402


class TimingTables(unittest.TestCase):
    def test_preview_rebuild_reads_replaced_tables(self):
        with TemporaryDirectory() as directory, patch.object(mkdocs_timing, "TABLES", Path(directory)):
            (Path(directory) / "NoGateList.generated.inc").write_text("", encoding="utf-8")

            def write_tables(key):
                for _, filename, _ in mkdocs_timing.PROFILES:
                    (Path(directory) / filename).write_text(
                        '{"' + key + '", [] {\n'
                        '    Arc{"A", "OUT", {1, 2}},\n'
                        '}()}\n', encoding="utf-8")

            try:
                write_tables("preview.old")
                mkdocs_timing.on_pre_build({})
                self.assertIn("<table", mkdocs_timing.render("preview.old"))
                write_tables("preview.new")
                mkdocs_timing.on_pre_build({})
                self.assertIn("<table", mkdocs_timing.render("preview.new"))
                with self.assertRaises(ValueError):
                    mkdocs_timing.render("preview.old")
            finally:
                mkdocs_timing.on_pre_build({})

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
        # The 8-bit ripple adder: A -> C_OUT, as stored for each profile.
        html = mkdocs_timing.render("word.ripple-carry-adder.8.behavioral")
        row = re.search(r"A → C_OUT</th>(.*?)</tr>", html).group(1)
        self.assertEqual(re.findall(r"<td>(.*?)</td>", row), ["2–17", "102–936", "71–652"])
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

    def test_selected_rows(self):
        # One chosen path: its latest change in each profile.
        html = mkdocs_timing.render_rows("Carry | word.ripple-carry-adder.8.behavioral | A → C_OUT")
        self.assertEqual(re.findall(r"<td>(.*?)</td>", html), ["17", "936", "652"])
        with self.assertRaises(ValueError):
            mkdocs_timing.render_rows("Carry | word.ripple-carry-adder.8.behavioral | A → Nothing")
        with self.assertRaises(ValueError):
            mkdocs_timing.render_rows("missing separators")
        for page in sorted((ROOT / "site-docs").rglob("*.md")):
            for spec in mkdocs_timing.ROWS_MARKER.findall(page.read_text(encoding="utf-8")):
                with self.subTest(page=str(page.relative_to(ROOT))):
                    mkdocs_timing.render_rows(spec)

    def test_component_timing_stands_alone(self):
        # Timing is a paragraph that ends a component's implementation, not a
        # heading of its own, and it says what it has to say on the spot
        # instead of sending the reader to the timing chapters.
        timing_pages = re.compile(r"\]\((?:\.\./)*(?:timing\.md|memory/timing\.md|4\.2/timing\.md)[#)]"
                                  r"|\]\(timing\.md[#)]|\]\(\.\./timing\.md[#)]")
        paragraphs = 0
        for page in sorted((ROOT / "site-docs").rglob("*.md")):
            text = page.read_text(encoding="utf-8")
            self.assertNotIn("\n### Timing\n", text, page)
            for block in re.findall(r"(?ms)^(?:<a id=\"[^\"]+\"></a>)?\*\*Timing\.\*\* .*?(?=^#{1,4} )", text):
                paragraphs += 1
                with self.subTest(page=str(page.relative_to(ROOT))):
                    self.assertIsNone(timing_pages.search(block), block[:80])
        # New component chapters can add timing paragraphs; validate every
        # paragraph above while retaining the original coverage floor.
        self.assertGreaterEqual(paragraphs, 16)


if __name__ == "__main__":
    unittest.main()
