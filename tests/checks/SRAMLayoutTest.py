"""Pin-safe, column-aligned SRAM layout and non-SRAM preservation checks."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import sram_layout
import circuit_backend as backend


class SRAMLayoutTest(unittest.TestCase):
    def test_geometry(self):
        for width, depth, k in ((8, 4, 2), (16, 6, 2), (32, 8, 4)):
            root = backend.create_sram("M", width, depth, k)
            with tempfile.TemporaryDirectory() as tmp:
                manager = server.LayoutManager(Path(tmp) / "layout.json")
                def walk(component):
                    yield component
                    for child in component.get_children():
                        yield from walk(child)
                for component in walk(root):
                    manager.ensure_component_layout_defaults(component)
                for component in walk(root):
                    children = component.get_children()
                    aspects = {c.get_name(): manager.aspect_for_component(c) for c in children}
                    aspect = manager.aspect_for_component(component)
                    placements = sram_layout.placements(component.get_type_name(), aspects, aspect) if children else None
                    if placements is None:
                        continue
                    self.assertEqual(placements, server._layered_graph_layout(
                        component, list(children), aspect,
                        manager.visual_ratio_for_component(component, "title_bar_ratio") / aspect,
                        {c.get_id(): aspects[c.get_name()] for c in children},
                        manager.visual_ratio_for_component(component, "boundary_area_ratio"),
                        manager.visual_ratio_for_component(component, "pin_size_ratio")))
                    rectangles = []
                    for child in children:
                        name = child.get_name()
                        x, y = placements[name]["rel_pos"]
                        w = placements[name]["rel_width"]
                        h = w * aspects[name] / aspect
                        self.assertGreaterEqual(x, 0)
                        self.assertGreaterEqual(y, 0)
                        self.assertLessEqual(x + w, 1)
                        self.assertLessEqual(y + h, 1)
                        rectangles.append((name, x, y, w, h))
                    for i, a in enumerate(rectangles):
                        for b in rectangles[i + 1:]:
                            self.assertFalse(min(a[1]+a[3], b[1]+b[3]) > max(a[1], b[1]) and
                                             min(a[2]+a[4], b[2]+b[4]) > max(a[2], b[2]), (component.get_type_name(), a, b))
                    if component.get_type_name().startswith("SRAMStorageBank<"):
                        for r in range(depth // k):
                            row = next(c for c in children if c.get_name() == f"ROW_{r}")
                            rp = placements[row.get_name()]
                            cells = sram_layout.placements(row.get_type_name(),
                                {c.get_name(): manager.aspect_for_component(c) for c in row.get_children()}, manager.aspect_for_component(row))
                            for c in range(width * k):
                                cell, pair = cells[f"CELL_{c}"], placements[f"BITLINE_{c}"]
                                center = rp["rel_pos"][0] + rp["rel_width"] * (cell["rel_pos"][0] + cell["rel_width"] / 2)
                                self.assertAlmostEqual(center, pair["rel_pos"][0] + pair["rel_width"] / 2)

    def test_shipped_layout(self):
        data = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        for kind in ("SRAMRow<2>", "SRAMRow<16>", "SRAMStorageBank<8,4,2>", "SRAM<8>"):
            entry = data["type_layouts"][kind]
            self.assertEqual(entry["layout_revision"], "sram-matrix-v1")
            self.assertIn("previous_default", entry)
        row = data["type_layouts"]["SRAMRow<16>"]["children"]
        self.assertEqual(len({v["rel_pos"][1] for v in row.values()}), 1)
        self.assertEqual(sorted(row, key=lambda n: row[n]["rel_pos"][0]), [f"CELL_{i}" for i in range(16)])
        for kind in ("FFWord<8>", "ALU8", "Rewire", "SRAM8"):
            self.assertEqual(sram_layout.defaults(kind), {})


if __name__ == "__main__":
    unittest.main()
