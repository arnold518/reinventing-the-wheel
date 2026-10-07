"""Stage-order floorplan, pin clearance, migration scope and repeatability."""
from pathlib import Path
import json
import runpy
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh


class SingleCycleLayoutTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = server.circuit_backend.create_layout_topology("RV32ISingleCycleCoreTest")
        cls.manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        cls.children = list(cls.root.get_children())

    def test_order_and_clearance_across_proportions(self):
        args = list(refresh.layout_arguments(self.manager, self.root, self.children))
        for aspect in (.42, .52, 1.0, 2.0):
            for mixed in (False, True):
                args[2] = aspect
                args[3] = .045 / aspect
                args[4] = {c.get_id(): (.18 + i*.37 if mixed else self.manager.aspect_for_component(c))
                           for i, c in enumerate(self.children)}
                args[7] = {c.get_id(): (.20 if mixed else .025) for c in self.children}
                placements = server._custom_child_layout(*args)
                self.assertEqual(set(placements), {c.get_name() for c in self.children})
                rects = {}
                for c in self.children:
                    p = placements[c.get_name()]
                    x, y = p["rel_pos"]
                    w = p["rel_width"]
                    h = w * args[4][c.get_id()] / aspect
                    stub = w * args[7][c.get_id()]
                    self.assertGreater(w, 0)
                    self.assertGreaterEqual(x-stub, args[5]-1e-9)
                    self.assertLessEqual(x+w+stub, 1-args[5]+1e-9)
                    self.assertGreaterEqual(y-stub/aspect, args[3]-1e-9)
                    self.assertLessEqual(y+h+stub/aspect, 1+1e-9)
                    rects[c.get_name()] = (x-stub, y-stub/aspect, x+w+stub, y+h+stub/aspect)
                order = server.SINGLE_CYCLE_STAGE_ORDER
                for a, b in zip(order, order[1:]):
                    self.assertLess(rects[a][2], rects[b][0])
                self.assertLess(max(rects[n][3] for n in order),
                                min(rects[n][1] for n in ("NEXT_PC", "EXECUTION_STATUS")))
                self.assertLess(rects["NEXT_PC"][2], rects["EXECUTION_STATUS"][0])

    def test_unknown_topology_is_not_forced_into_floorplan(self):
        args = list(refresh.layout_arguments(self.manager, self.root, self.children))
        args[1] = self.children[:-1]
        self.assertIsNone(server._custom_child_layout(*args))

    def test_saved_defaults_and_regeneration(self):
        generated = server._custom_child_layout(*refresh.layout_arguments(self.manager, self.root, self.children))
        self.assertEqual(generated, self.manager.type_layouts["RV32ISingleCycleCore"]["children"])
        # A broad automatic refresh must recognize the core as a floorplan.
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.json"
            shutil.copyfile(self.manager.filepath, path)
            before = path.read_bytes()
            namespace = runpy.run_path(str(ROOT / "tools/generate-single-cycle-core-layout.py"))
            namespace["refresh_layout"](path)
            self.assertEqual(path.read_bytes(), before)
            report = refresh.refresh_automatic(path, ["rv32i-core"])
            self.assertIn("type/RV32ISingleCycleCore", report["preserved_custom"])
            after = json.loads(path.read_text())
            self.assertEqual(after["type_layouts"]["RV32ISingleCycleCore"],
                             self.manager.type_layouts["RV32ISingleCycleCore"])


if __name__ == "__main__":
    unittest.main()
