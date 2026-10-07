"""Only new memory families: bounds, matrix alignment, defaults and bindings."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(ROOT / "visualizer-v2"))
import circuit_backend as backend
import server
import memory_device_layout as geometry
from palette import color_for_layout_type, DEFAULT_COMPONENT_COLOR

PROBE = sys.argv.pop(1) if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")


class MemoryDeviceLayoutTest(unittest.TestCase):
    def test_memory_history_and_palette_in_native_session(self):
        for scenario, test in (("dram", "DRAMTest"), ("rom", "ROMTest"), ("nor-flash", "NORFlashTest")):
            with tempfile.TemporaryDirectory() as tmp:
                manager = server.LayoutManager(Path(tmp) / "layout.json")
                session = server.CircuitSession(scenario, manager,
                                               profile_overrides={"MEMORY_ROOT": "behavioral"})
                for index in (0, len(session.timestamps)-1):
                    state = session.component_state_at_index(index, "MEMORY_ROOT")
                    self.assertTrue(state["supported"])
                    self.assertEqual(len(state["windowWords"]), 4)
                    native = json.loads(subprocess.check_output([
                        PROBE, "component", test, scenario, str(index), "MEMORY_ROOT",
                        "--override", "MEMORY_ROOT", "behavioral"], text=True))
                    self.assertEqual(state, native)
                    words = {word["address"]: word["bits"] for word in state["windowWords"]}
                    if index == 0:
                        self.assertEqual(words[0], {"dram": "XXXXXXXX", "rom": "10100101",
                                                    "nor-flash": "11111111"}[scenario])
                    else:
                        self.assertEqual(words[0], {"dram": "01011010", "rom": "10100101",
                                                    "nor-flash": "11111111"}[scenario])
                        if scenario == "nor-flash":
                            self.assertEqual(words[2], "01111000")
                    if scenario != "rom":
                        self.assertIsNone(state["activeAddress"])
                        self.assertIn("CMD", state["ports"])
                        self.assertIn("ERROR", state["ports"])
                kind = session.root.get_type_name()
                self.assertNotEqual(color_for_layout_type(kind), list(DEFAULT_COMPONENT_COLOR))
        for family in ("DRAM", "ROM", "NORFlash"):
            self.assertNotEqual(color_for_layout_type(family + "Cell"),
                                color_for_layout_type(family + "Row<16>"))

    def test_matrix_geometry(self):
        for family in ("dram", "rom", "nor-flash"):
            for width, depth, k in ((8, 4, 2), (16, 6, 2), (32, 8, 4), (8, 8, 1), (8, 32, 2)):
                params = {"depth_words": str(depth), "words_per_row": str(k)}
                if family == "nor-flash":
                    params["block_words"] = str(k)
                root = backend.create_memory_device_component(family, f"device.{width}", "M", params)
                with tempfile.TemporaryDirectory() as tmp:
                    manager = server.LayoutManager(Path(tmp) / "layout.json")

                    def walk(c):
                        yield c
                        for child in c.get_children():
                            yield from walk(child)

                    for c in walk(root):
                        manager.ensure_component_layout_defaults(c)
                    for c in walk(root):
                        children = c.get_children()
                        aspects = {child.get_name(): manager.aspect_for_component(child) for child in children}
                        aspect = manager.aspect_for_component(c)
                        layout = geometry.placements(c.get_type_name(), aspects, aspect)
                        if layout is None:
                            continue
                        rectangles = []
                        for name, value in layout.items():
                            x, y = value["rel_pos"]
                            w = value["rel_width"]
                            h = w * aspects[name] / aspect
                            self.assertGreaterEqual(min(x, y), 0)
                            self.assertLessEqual(max(x + w, y + h), 1, (c.get_type_name(), name, value, h))
                            rectangles.append((name, x, y, w, h))
                        for i, a in enumerate(rectangles):
                            for b in rectangles[i + 1:]:
                                self.assertFalse(min(a[1]+a[3], b[1]+b[3]) > max(a[1], b[1]) and
                                                 min(a[2]+a[4], b[2]+b[4]) > max(a[2], b[2]), (c.get_type_name(), a, b))
                        if "StorageBank<" in c.get_type_name():
                            for r in range(depth // k):
                                row = next(child for child in children if child.get_name() == f"ROW_{r}")
                                rp = layout[row.get_name()]
                                cells = geometry.placements(row.get_type_name(),
                                    {cell.get_name(): manager.aspect_for_component(cell) for cell in row.get_children()},
                                    manager.aspect_for_component(row))
                                self.assertEqual(sorted(cells, key=lambda n: cells[n]["rel_pos"][0]),
                                                 [f"CELL_{i}" for i in range(width*k)])
                                for col in range(width * k):
                                    cp, sensor = cells[f"CELL_{col}"], layout[f"COLUMN_{col}"]
                                    center = rp["rel_pos"][0] + rp["rel_width"] * (cp["rel_pos"][0] + cp["rel_width"] / 2)
                                    self.assertAlmostEqual(center, sensor["rel_pos"][0] + sensor["rel_width"] / 2)

    def test_bindings_and_limits(self):
        for family in ("dram", "rom", "nor-flash"):
            for width in (8, 16, 32):
                for mode in ("structural", "behavioral"):
                    component = backend.create_memory_device_component(family, f"device.{width}", "M", fidelity=mode)
                    self.assertEqual(component.get_output_pins()["READ_DATA"].get_width(), width)
                    self.assertEqual(bool(component.get_children()), mode == "structural")
            for p in ({"depth_words": "0"}, {"depth_words": "1048577"}, {"words_per_row": "3"}, {"oops": "1"}):
                with self.assertRaises(ValueError):
                    backend.create_memory_device_component(family, "device.8", "M", p)
            with self.assertRaises(ValueError):
                backend.create_memory_device_component(family, "device.32", "M", {"depth_words": "65536"})
            large = backend.create_memory_device_component(family, "device.32", "M", {"depth_words": "65536"}, "behavioral")
            self.assertFalse(large.get_children())

    def test_saved_defaults(self):
        data = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        for family in ("DRAM", "ROM", "NORFlash"):
            row = data["type_layouts"][family + "Row<16>"]["children"]
            self.assertEqual(sorted(row, key=lambda n: row[n]["rel_pos"][0]), [f"CELL_{i}" for i in range(16)])
            bank = data["type_layouts"][family + "StorageBank<8,4,2>"]["children"]
            self.assertLess(bank["ROW_0"]["rel_pos"][1], bank["ROW_1"]["rel_pos"][1])
        for scenario in ("dram-storage-bank", "rom-storage-bank", "nor-flash-storage-bank"):
            for key, entry in data["root_layouts"].items():
                if key.split("@")[0] == scenario:
                    for child in entry.get("children", {}).values():
                        self.assertLess(child["rel_pos"][1], 1, (key, child))
        for kind in ("FFWord<8>", "SRAMRow<16>", "ALU8", "Rewire"):
            self.assertEqual(geometry.defaults(kind), {})

    def test_first_encounter_bank_layout(self):
        for scenario in ("dram-storage-bank", "rom-storage-bank", "nor-flash-storage-bank"):
            with tempfile.TemporaryDirectory() as tmp:
                manager = server.LayoutManager(Path(tmp) / "empty.json")
                session = server.CircuitSession(scenario, manager)
                entry = manager.get_root_layout_for_component(session.layout_key, session.root)
                for name, child in entry["children"].items():
                    self.assertLess(child["rel_pos"][1], 1, (scenario, name, child))

    def test_peripheral_bit_and_slot_lanes(self):
        for width in (8, 16, 32):
            for slots in (1, 2, 4, 16):
                for kind in ("column-mux", "write-distributor"):
                    component = backend.create_memory_device_component(
                        "matrix", f"{kind}.{width}", "M", {"words_per_row": str(slots)})
                    with tempfile.TemporaryDirectory() as tmp:
                        manager = server.LayoutManager(Path(tmp) / "layout.json")
                        manager.ensure_component_layout_defaults(component)
                        children = component.get_children()
                        for child in children:
                            manager.ensure_component_layout_defaults(child)
                        aspects = {c.get_name(): manager.aspect_for_component(c) for c in children}
                        aspect = manager.aspect_for_component(component)
                        layout = geometry.placements(component.get_type_name(), aspects, aspect)
                        self.assertIsNotNone(layout, (kind, width, slots, component.get_type_name(), set(aspects)))
                        self.assertEqual(set(layout), set(aspects))
                        for slot in range(slots):
                            names = [f"SELECT_{slot}_{bit}" if kind == "column-mux"
                                     else f"WRITE_SLOT_{slot}_BYTE_{bit}"
                                     for bit in range(width if kind == "column-mux" else width // 8)]
                            self.assertEqual(sorted(names, key=lambda n: layout[n]["rel_pos"][0]), names)
                            self.assertEqual(len({layout[n]["rel_pos"][1] for n in names}), 1)
                        rectangles = []
                        for name, p in layout.items():
                            x, y = p["rel_pos"]
                            w, h = p["rel_width"], p["rel_width"] * aspects[name] / aspect
                            self.assertGreaterEqual(min(x, y), 0)
                            self.assertLessEqual(max(x+w, y+h), 1.000001)
                            rectangles.append((name,x,y,w,h))
                        for i, a in enumerate(rectangles):
                            for b in rectangles[i+1:]:
                                self.assertFalse(min(a[1]+a[3],b[1]+b[3]) > max(a[1],b[1]) and
                                                 min(a[2]+a[4],b[2]+b[4]) > max(a[2],b[2]), (a,b))


if __name__ == "__main__":
    unittest.main()
