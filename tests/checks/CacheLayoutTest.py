"""Cache floorplans align each way and fit across supported word widths."""

from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PROBE = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else ROOT / "build-cleanup/tests/VisualizerSessionProbe"
sys.path.append(str(ROOT / "visualizer-v2"))
import circuit_backend  # Preload the isolated build before server adds the live module path.
import cache_layout
import server


class CacheLayoutTest(unittest.TestCase):
    def assert_geometry(self, layout, aspects, parent_aspect):
        self.assertEqual(set(layout), set(aspects))
        boxes = {}
        for name, placement in layout.items():
            x, y = placement["rel_pos"]
            width = placement["rel_width"]
            height = width * aspects[name] / parent_aspect
            self.assertGreaterEqual(min(x, y), 0, name)
            self.assertLessEqual(max(x + width, y + height), 1, name)
            boxes[name] = (x, y, width, height)
        for name, (x, y, width, height) in boxes.items():
            for other, (ox, oy, ow, oh) in boxes.items():
                if name >= other:
                    continue
                self.assertTrue(x + width <= ox or ox + ow <= x or
                                y + height <= oy or oy + oh <= y, (name, other))
        return boxes

    def test_parameterized_bank_alignment(self):
        for word_width in (8, 16, 32):
            for ways in (1, 2):
                for parent_aspect in (0.8, 1.76, 3.0):
                    for tag_aspect, data_aspect in ((0.8, 0.8), (0.8, 1.4), (1.4, 0.8)):
                        aspects = {"CONTROLLER": 3.715}
                        for way in range(ways):
                            aspects[f"TAG_STORE_{way}"] = tag_aspect
                            aspects[f"DATA_STORE_{way}"] = data_aspect
                        if ways == 2:
                            for prefix in ("TAG", "DATA"):
                                aspects[f"{prefix}_SELECT_ZERO"] = 0.38
                                for kind in ("REQ_VALID", "RSP_READY"):
                                    for way in range(2):
                                        aspects[f"{prefix}_{kind}_WAY_{way}"] = 0.495
                                for field in ("REQ_READY", "RSP_READ_DATA", "RSP_STATUS", "RSP_VALID"):
                                    aspects[f"{prefix}_RESPONSE_{field}"] = 0.8
                        else:
                            aspects.update({"TAG_SELECT_ZERO": 0.38, "DATA_SELECT_ZERO": 0.38})
                        layout = cache_layout.placements(f"Cache<{word_width}>", aspects, parent_aspect)
                        self.assertIsNotNone(layout)
                        boxes = self.assert_geometry(layout, aspects, parent_aspect)
                        for way in range(ways):
                            tag = boxes[f"TAG_STORE_{way}"]
                            data = boxes[f"DATA_STORE_{way}"]
                            self.assertAlmostEqual(tag[1] + tag[3] / 2, data[1] + data[3] / 2)
                            self.assertLess(tag[0] + tag[2], data[0])
                        if ways == 2:
                            self.assertLess(boxes["TAG_STORE_0"][1] + boxes["TAG_STORE_0"][3],
                                            boxes["TAG_STORE_1"][1])

    def test_registered_scenario_uses_cache_floorplan(self):
        manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        for test, alias in (("CacheComponentScenarioTest", "cache-read-hit"),
                            ("CacheDirectMappedScenarioTest", "cache-direct-mapped"),
                            ("CacheScalableScenarioTest", "cache-scalable-two-way")):
            with self.subTest(scenario=alias):
                root = circuit_backend.create_layout_topology(test)
                children = list(root.get_children())
                aspects = {child.get_name(): manager.aspect_for_component(child) for child in children}
                parent_aspect = manager.aspect_for_component(root)
                layout = server._custom_child_layout(
                    root, children, parent_aspect, 0.02,
                    {child.get_id(): aspects[child.get_name()] for child in children}, 0.035, 0.006)
                self.assertIsNotNone(layout)
                self.assert_geometry(layout, aspects, parent_aspect)
                key = server.scenario_layout_key(alias, root)
                saved = {
                    child.get_name(): manager.get_child_layout_for_component(
                        root, child.get_name(), scenario_key=key, is_root=True)
                    for child in children
                }
                self.assertEqual({name: (item["rel_pos"], item["rel_width"]) for name, item in saved.items()},
                                 {name: (item["rel_pos"], item["rel_width"]) for name, item in layout.items()})
                controller = next(child for child in children if child.get_name() == "CONTROLLER")
                self.assertTrue({"ADDRESS_PATH", "PHASES"} <=
                                {child.get_name() for child in controller.get_children()})
                for component in (controller, *[child for child in controller.get_children()
                                                if child.get_name() in {"ADDRESS_PATH", "PHASES"}]):
                    subchildren = list(component.get_children())
                    saved = {child.get_name(): manager.get_child_layout_for_component(component, child.get_name())
                             for child in subchildren}
                    subaspects = {child.get_name(): manager.aspect_for_component(child) for child in subchildren}
                    self.assert_geometry(saved, subaspects, manager.aspect_for_component(component))

    def test_two_level_scenario_has_both_caches_and_ram(self):
        root = circuit_backend.create_layout_topology("CacheTwoLevelScenarioTest")
        children = {child.get_name() for child in root.get_children()}
        self.assertTrue({"CACHE", "LOWER_CACHE", "BACKING"} <= children)
        manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
        key = server.scenario_layout_key("cache-two-level", root)
        for name in ("CACHE", "LOWER_CACHE", "BACKING"):
            self.assertIsNotNone(manager.get_child_layout_for_component(
                root, name, scenario_key=key, is_root=True))

    def test_native_and_portable_cache_layout_keys_agree(self):
        with tempfile.TemporaryDirectory() as directory:
            native = server.CircuitSession(
                "cache-read-hit", server.LayoutManager(Path(directory) / "layout.json"))
            portable = json.loads(subprocess.check_output(
                [str(PROBE), "topology", "CacheComponentScenarioTest", "cache-read-hit"], text=True))
            native_keys = {
                item["id"]: item.get("layoutType", item["type"])
                for item in native.topology_response()["components"]
                if item["type"].startswith("Cache")
            }
            portable_keys = {
                item["id"]: item.get("layoutType", item["type"])
                for item in portable["components"]
                if item["type"].startswith("Cache")
            }
            self.assertEqual(native_keys, portable_keys)


if __name__ == "__main__":
    unittest.main()
