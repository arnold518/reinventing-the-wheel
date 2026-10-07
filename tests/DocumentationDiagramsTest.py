"""Static figure/source/hook checks; no MkDocs, browser, or simulator required."""

from pathlib import Path
import re
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mkdocs_diagrams import MARKER, inline_diagrams
from render_documentation_diagrams import DESTINATION, figures

DOCS = ROOT / "site-docs"
NS = "{http://www.w3.org/2000/svg}"
PAGES = {
    "memory/3.1/index.md": ["memory-storage", "memory-endpoints"],
    "memory/3.1.1.md": ["ff-memory"],
    "rv32i/4.3/register.md": ["register32"],
    "rv32i/4.3/register-file.md": ["register-file"],
    "rv32i/4.3/memory-access.md": ["memory-access"],
    "rv32i/4.4/index.md": ["single-cycle-stages", "single-cycle-writeback",
                            "single-cycle-next-pc", "single-cycle-completion"],
    "rv32i/4.5/index.md": ["baseline-pipeline"],
    "rv32i/4.6/index.md": ["advanced-prediction"],
}


class DocumentationDiagramsTest(unittest.TestCase):
    def test_generated_assets_match_source(self):
        self.assertEqual({f.slug for f in figures()}, {s for slugs in PAGES.values() for s in slugs})
        for figure in figures():
            with self.subTest(figure=figure.slug):
                self.assertEqual((DESTINATION / (figure.slug + ".html")).read_text(), figure.html())

    def test_accessibility_and_self_contained_svg(self):
        for figure in figures():
            with self.subTest(figure=figure.slug):
                html = figure.html()
                svg_source, = re.findall(r"(<svg\b.*?</svg>)", html, re.S)
                svg = ET.fromstring(svg_source)
                self.assertEqual(svg.attrib["role"], "img")
                self.assertEqual(svg[0].tag, NS + "title")
                ids = [node.attrib["id"] for node in svg.iter() if "id" in node.attrib]
                self.assertEqual(len(ids), len(set(ids)))
                self.assertEqual(svg.attrib["aria-labelledby"].split(),
                                 [figure.slug + "-title", figure.slug + "-desc"])
                for tag in ("title", "desc"):
                    self.assertTrue(svg.find(NS + tag).text.strip())
                self.assertIn('role="region"', html)
                self.assertIn('tabindex="0"', html)
                self.assertIn("<figcaption>", html)
                for node in svg.iter():
                    self.assertNotIn(node.tag, [NS + name for name in ("script", "image", "foreignObject", "iframe")])
                    self.assertFalse(any(key.lower().startswith("on") for key in node.attrib))
                for target in re.findall(r"url\(([^)]+)\)", svg_source):
                    self.assertTrue(target.startswith("#"), target)
                    self.assertIn(target[1:], ids)

    def test_register_figure_covers_the_width_parametric_family(self):
        html = next(f for f in figures() if f.slug == "register32").html()
        for label in ("Register&lt;W&gt;", "W × FlipFlopCell", "BIT_(W−1)", "W=1"):
            self.assertIn(label, html)
        self.assertNotIn("BIT_31", html)

    def test_memory_architecture_is_device_composition_not_processor_paths(self):
        figure = next(f for f in figures() if f.slug == "memory-endpoints")
        html = figure.html()
        svg = ET.fromstring(re.search(r"(<svg\b.*?</svg>)", html, re.S).group(1))
        nodes = {node.attrib["data-node"]: node for node in svg.iter() if "data-node" in node.attrib}

        def contains(parent, child):
            p, c = nodes[parent].attrib, nodes[child].attrib
            return (float(p["x"]) <= float(c["x"]) and float(p["y"]) <= float(c["y"])
                    and float(c["x"]) + float(c["width"]) <= float(p["x"]) + float(p["width"])
                    and float(c["y"]) + float(c["height"]) <= float(p["y"]) + float(p["height"]))

        for child in ("controller", "native-device"):
            self.assertTrue(contains("endpoint", child))
        for child in ("expanded-device", "container"):
            self.assertTrue(contains("native-device", child))
        self.assertFalse(contains("controller", "native-device"))
        self.assertFalse(contains("endpoint", "transaction-interface"))
        for label in ("MemoryEndpoint", "MemoryController", "MemoryContainer", "REQ_* / RSP_*",
                      "FF", "SRAM", "DRAM", "ROM", "NOR flash", "alternatives", "BehavioralMemoryEndpoint"):
            self.assertIn(label, html)
        for processor_term in ("RV32I", "single-cycle", "Single-cycle", "pipeline", "direct-core"):
            self.assertNotIn(processor_term, html)

    def test_pages_inline_figures_without_touching_code_or_examples(self):
        for path, slugs in PAGES.items():
            with self.subTest(page=path):
                source = (DOCS / path).read_text()
                self.assertEqual(MARKER.findall(source), slugs)
                self.assertNotIn("```mermaid", source)
                rendered = inline_diagrams(source, DOCS)
                self.assertEqual(rendered.count('<figure class="circuit-figure"'), len(slugs))
                self.assertNotIn("<!-- diagram:", rendered)
                self.assertNotIn("<!DOCTYPE", rendered)
                self.assertNotIn("fonts.googleapis.com", rendered)
                for pattern in (r"```cpp\n.*?\n```", r"<!-- simulator-examples: .*? -->", r"<iframe\b.*?</iframe>"):
                    self.assertEqual(re.findall(pattern, source, re.S), re.findall(pattern, rendered, re.S))
                ids = re.findall(r'\bid="([^"]+)"', rendered)
                self.assertEqual(len(ids), len(set(ids)))

    def test_invalid_missing_and_duplicate_references_fail(self):
        for marker in ("../escape", "BadName", "", "no-such-diagram"):
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                inline_diagrams(f"<!-- diagram: {marker} -->", DOCS)
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            inline_diagrams("<!-- diagram: memory-storage -->\n<!-- diagram: memory-storage -->", DOCS)

    def test_malformed_asset_fails(self):
        with tempfile.TemporaryDirectory(prefix="ritw-diagram-test-") as directory:
            assets = Path(directory) / "assets/diagrams"
            assets.mkdir(parents=True)
            for source in ("no marked region", '<!-- diagram:start --><figure data-diagram="wrong"></figure><!-- diagram:end -->'):
                (assets / "test.html").write_text(source)
                with self.assertRaisesRegex(ValueError, "Invalid figure"):
                    inline_diagrams("<!-- diagram: test -->", directory)

    def test_hook_enabled_for_both_sites(self):
        for name in ("mkdocs.yml", "mkdocs.preview.yml"):
            self.assertIn("  - tools/mkdocs_diagrams.py", (ROOT / name).read_text())


if __name__ == "__main__":
    unittest.main()
