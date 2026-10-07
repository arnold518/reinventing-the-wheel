"""Selective default refresh: clearance, topology-only construction and preservation."""
import json
from pathlib import Path
import runpy
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh
import rv32i_layout


class AutomaticLayoutTest(unittest.TestCase):
    def test_routing_floorplans(self):
        manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        root = server.circuit_backend.create_layout_topology("RV32IFiveStageCoreProgramTest/program-01")
        checked = set()
        for component, _ in refresh._walk(root):
            kind = component.get_type_name()
            if not (kind.startswith("RV32IDecodeControlField<") or kind == "RV32IFetchRequest"):
                continue
            children = list(component.get_children())
            view = refresh.ProfileLayoutView(manager, server._component_profile_fingerprint(component))
            generated = server._custom_child_layout(*refresh.layout_arguments(view, component, children))
            self.assertEqual(set(generated), {c.get_name() for c in children})
            refresh._assert_child_bounds(view, component, children, generated)
            refresh._assert_no_child_overlap(view, component, children, generated, 1, "")
            for name, placement in generated.items():
                saved = view.get_child_layout_for_component(component, name)
                self.assertEqual(saved["rel_pos"], placement["rel_pos"])
                self.assertEqual(saved["rel_width"], placement["rel_width"])
            if kind == "RV32IDecodeControlField<MEM_WRITE>":
                # Two real OR levels use the page, not two of six reserved columns.
                first, second = (generated[f"BIT_0_OR_{level}_0"] for level in (0, 1))
                self.assertGreater(first["rel_width"], .18)
                self.assertGreater(second["rel_width"], .18)
                self.assertLess(first["rel_pos"][0] + first["rel_width"], second["rel_pos"][0])
            if kind == "RV32IFetchRequest":
                address, select, match = (generated[n] for n in ("REQUEST_ADDR", "SELECT_ADDR", "TAG_MATCH"))
                self.assertLess(address["rel_pos"][0], select["rel_pos"][0])
                self.assertLess(select["rel_pos"][0], match["rel_pos"][0])
                self.assertGreater(select["rel_width"], .15)
                self.assertLess(select["rel_pos"][1], generated["START"]["rel_pos"][1])
                self.assertLess(generated["BUSY_NEXT"]["rel_pos"][0], generated["BUSY"]["rel_pos"][0])
            checked.add(kind)
        self.assertEqual(len(checked), 19)

    def test_routing_refresh_preserves_manual_and_unrelated(self):
        tool = runpy.run_path(str(ROOT / "tools/refresh-rv32i-routing-layouts.py"))
        original = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        rewire = original["type_layouts"][tool["REWIRE"]]
        rewire.pop("layout_revision", None)
        rewire["aspect_ratio"] = 4.865
        # A profile inherits the old type proportions but overrides color.
        original["profile_layouts"]["routing-test"] = {tool["REWIRE"]: {"color": [1, 2, 3, 190]}}
        original["type_layouts"]["RV32IFetchRequest"]["layout_mode"] = "manual"
        kind = "RV32IDecodeControlField<MEM_WRITE>"
        original["type_layouts"][kind]["children"]["BIT_0_OR_0_0"]["rel_pos"] = [.01, .02]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.json"
            path.write_text(json.dumps(original))
            result = tool["refresh"](path)
            updated = json.loads(path.read_text())
            self.assertIn(f"explicit/{kind}", result["changed"])
            self.assertIn("explicit/RV32IFetchRequest", result["preserved_manual"])
            self.assertEqual(updated["type_layouts"]["RV32IFetchRequest"], original["type_layouts"]["RV32IFetchRequest"])
            profile = updated["profile_layouts"]["routing-test"][tool["REWIRE"]]
            self.assertEqual(profile["previous_default"]["aspect_ratio"], 4.865)
            self.assertEqual(profile["color"], [1, 2, 3, 190])
            for key, value in original["type_layouts"].items():
                if key != tool["REWIRE"] and not key.startswith("RV32IDecodeControlField<"):
                    self.assertEqual(updated["type_layouts"][key], value, key)
            self.assertEqual(updated["root_layouts"], original["root_layouts"])
            snapshot = path.read_bytes()
            self.assertFalse(tool["refresh"](path)["changed"])
            self.assertEqual(path.read_bytes(), snapshot)

    def test_balanced_matcher_floorplan(self):
        manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        for count in (1, 2, 3, 5, 8, 13, 32):
            mask = (1 << count) - 1
            for value in (0, mask):
                component = server.circuit_backend.RV32IBitPatternMatcher("M", mask, value)
                manager.ensure_component_layout_defaults(component)
                children = list(component.get_children())
                aspect = manager.aspect_for_component(component)
                generated = rv32i_layout.placements(component.get_type_name(),
                    {c.get_name(): manager.aspect_for_component(c) for c in children}, aspect)
                self.assertIsNotNone(generated)
                self.assertEqual(set(generated), {c.get_name() for c in children})
                refresh._assert_child_bounds(manager, component, children, generated)
                refresh._assert_no_child_overlap(manager, component, children, generated, 1, "")
                levels = {}
                for name, placement in generated.items():
                    if name.startswith("MATCH_TREE_"):
                        levels.setdefault(int(name.split("_")[2]), []).append(placement)
                for level in range(len(levels) - 1):
                    self.assertLess(max(p["rel_pos"][0] + p["rel_width"] for p in levels[level]),
                                    min(p["rel_pos"][0] for p in levels[level + 1]))

    def test_optimized_refresh_preserves_other_layouts(self):
        tool = runpy.run_path(str(ROOT / "tools/refresh-rv32i-optimized-layouts.py"))
        original = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        original["type_layouts"]["RV32IPredictionPath"]["layout_mode"] = "manual"
        original["type_layouts"]["RV32IExecuteStage"]["children"]["DATAPATH"]["rel_pos"] = [.01, .02]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.json"
            path.write_text(json.dumps(original))
            result = tool["refresh"](path)
            updated = json.loads(path.read_text())
            self.assertIn("explicit/RV32IExecuteStage", result["changed"])
            self.assertIn("explicit/RV32IPredictionPath", result["preserved_manual"])
            self.assertEqual(updated["type_layouts"]["RV32IPredictionPath"], original["type_layouts"]["RV32IPredictionPath"])
            self.assertEqual(set(updated["type_layouts"]), set(original["type_layouts"]))
            for kind, entry in original["type_layouts"].items():
                if kind.split("<")[0] not in tool["KINDS"]:
                    self.assertEqual(updated["type_layouts"][kind], entry, kind)
            snapshot = path.read_bytes()
            self.assertFalse(tool["refresh"](path)["changed"])
            self.assertEqual(path.read_bytes(), snapshot)

    def test_selective_refresh(self):
        original = json.loads((ROOT / "visualizer-v2/layout.json").read_text())
        # Simulate stale defaults, a deliberate handmade entry and an obsolete
        # profile shape. None may cause the entire store to be reset.
        original["type_layouts"]["RV32IMemoryLanes"]["children"]["LANE_0"]["rel_pos"][0] = .001
        # The selected pipeline topology no longer contains ALU32FlagUnit;
        # mark a component that is actually visited by this refresh.
        original["type_layouts"]["RV32IFiveStageCore"]["layout_mode"] = "manual"
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
            self.assertIn("type/RV32IFiveStageCore", result["preserved_manual"])
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
            self.assertIn("type/ALU32Core", result["automatic"])
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
