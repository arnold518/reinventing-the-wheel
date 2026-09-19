"""Selective default refresh: clearance, topology-only construction and preservation."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh


class AutomaticLayoutTest(unittest.TestCase):
    def test_selective_refresh(self):
        original = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        # Simulate stale defaults, a deliberate handmade entry and an obsolete
        # profile shape. None may cause the entire store to be reset.
        original["type_layouts"]["RV32IMemoryLanes"]["children"]["LANE_0"]["rel_pos"][0] = .001
        original["type_layouts"]["ALU32FlagUnit"]["layout_mode"] = "manual"
        original["profile_layouts"].setdefault("unrecognized-test", {})["RV32IMemoryLanes"] = {
            "children": {"RETIRED_CHILD": {"rel_pos": [.1, .2], "rel_width": .3}}}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.json"
            path.write_text(json.dumps(original))
            scenarios = ["rv32i-memory-lanes", "sram", "rv32i-five-stage-program3"]
            dry = refresh.refresh_automatic(path, scenarios, dry_run=True)
            self.assertEqual(json.loads(path.read_text()), original)
            result = refresh.refresh_automatic(path, scenarios)
            updated = json.loads(path.read_text())
            self.assertEqual(result, dry)
            self.assertIn("type/RV32IMemoryLanes", result["changed"])
            self.assertIn("type/ALU32FlagUnit", result["preserved_manual"])
            self.assertIn("profile/unrecognized-test/RV32IMemoryLanes", result["unmatched"])
            for kind in ["SRAM<8>", "SRAMRow<16>", "SRAMStorageBank<8,4,2>", "RV32IFiveStageCore", "RegisterFile32x32", "ALU32FlagUnit"]:
                self.assertEqual(updated["type_layouts"][kind], original["type_layouts"][kind], kind)
            for store in ["type_layouts", "root_layouts"]:
                self.assertEqual(set(updated[store]), set(original[store]))
                for key, old in original[store].items():
                    for field, value in old.items():
                        if field not in {"children", "automatic_layout_history", "automatic_layout_revision", "layout_mode"}:
                            self.assertEqual(updated[store][key][field], value, (store,key,field))
            # Refresh descendants, even inside a preserved pipeline floorplan.
            self.assertIn("type/ALU32", result["automatic"])
            snapshot = path.read_bytes()
            second = refresh.refresh_automatic(path, scenarios)
            self.assertFalse(second["changed"])
            self.assertEqual(path.read_bytes(), snapshot)

    def test_topology_only_matches_preview(self):
        def shape(root):
            # Root stimulus wires are deliberately omitted; internal wiring,
            # chosen implementations, pin shapes and profile IDs must agree.
            return [(c.get_id(), c.get_type_name(), server._component_profile_fingerprint(c),
                     [(p.get_id(), p.get_width()) for pins in (c.get_input_pins(),c.get_output_pins()) for p in pins.values()]
                        if hasattr(c,"get_input_pins") else [],
                     [(w.get_source_pin().get_id() if w.get_source_pin() else None,
                       [p.get_id() for p in w.get_sink_pins()]) for w in c.get_wires()
                      if any(p.get_owner().get_id() != c.get_id()
                             for p in [*w.get_sink_pins(), *([w.get_source_pin()] if w.get_source_pin() else [])])])
                    for c,_ in refresh._walk(root)]
        for name in ["RewireTest", "RV32IMemoryLanesTest", "SRAMTest", "RV32ISingleCycleMemoryStageTest",
                     "RV32ISingleCycleSystemTest/program-03", "RV32IFiveStageCoreProgramTest/program-03"]:
            topology = server.circuit_backend.create_layout_topology(name)
            test = server.circuit_backend.create_test_by_name(name)
            test.setup_circuit()
            self.assertEqual(shape(topology), shape(test.get_root()), name)

    def test_wide_child_pins_in_shallow_parent(self):
        root = server.circuit_backend.create_layout_topology("ALU32FlagUnitTest")
        children = list(root.get_children())
        manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        for aspect in [.45, .725, 2.0]:
            manager.type_layouts["ALU32FlagUnit"]["aspect_ratio"] = aspect
            arguments = refresh.layout_arguments(manager, root, children)
            generated = server._layered_graph_layout(*arguments)
            refresh._assert_no_child_overlap(manager,root,children,generated,1,"")
            refresh._assert_child_bounds(manager,root,children,generated)

    def test_mixed_grid_pin_footprints(self):
        class Child:
            def __init__(self, index): self.index = index
            def get_id(self): return f"ROOT.C{self.index}"
            def get_name(self): return f"C{self.index}"
        for count in (1, 2, 3, 8, 16, 32):
            children = [Child(i) for i in range(count)]
            for aspect in (.3, .7, 1, 2.5):
                sizes = {c.get_id(): [.2,.5,1.5][i%3] for i,c in enumerate(children)}
                stubs = {c.get_id(): [.055,.1,.16][i%3] for i,c in enumerate(children)}
                generated = server._balanced_grid_layout(children,aspect,.15,sizes,.025,.018,stubs)
                boxes = []
                for c in children:
                    geometry = generated[c.get_name()]
                    x,y = geometry["rel_pos"]; width = geometry["rel_width"]
                    stub = stubs[c.get_id()]*width
                    height = width*sizes[c.get_id()]/aspect; vertical = stub/aspect
                    self.assertGreaterEqual(x-stub, .025-1e-9)
                    self.assertLessEqual(x+width+stub, .975+1e-9)
                    self.assertGreaterEqual(y-vertical, .15-1e-9)
                    self.assertLessEqual(y+height+vertical, 1+1e-9)
                    boxes.append((x-stub,y-vertical,width+2*stub,height+2*vertical))
                for i,a in enumerate(boxes):
                    for b in boxes[i+1:]:
                        self.assertTrue(min(a[0]+a[2],b[0]+b[2])<=max(a[0],b[0])+1e-9
                            or min(a[1]+a[3],b[1]+b[3])<=max(a[1],b[1])+1e-9)


if __name__ == "__main__":
    unittest.main()
