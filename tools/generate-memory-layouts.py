"""Regenerate additive memory scenario defaults using the native layout engine.

Run from the repository root after building the Python module. Existing
placements are preserved by LayoutManager; only missing defaults are generated.
"""
from pathlib import Path
from copy import deepcopy
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "visualizer-v2"))
import server


layout = server.LayoutManager(root / "visualizer-v2" / "layout.json")
for scenario in ("ff-memory-container", "ff-word", "ff-storage-bank"):
    root_id = (
               "FF_WORD_ROOT" if scenario == "ff-word" else
               "FF_STORAGE_BANK_ROOT" if scenario == "ff-storage-bank" else "MEMORY_CONTAINER_ROOT")
    for fidelity in ("structural", "behavioral"):
        session = server.CircuitSession(scenario, layout, profile_overrides={root_id: fidelity})
        if fidelity == "structural" and scenario in ("ff-memory-container", "ff-word", "ff-storage-bank"):
            # A nested word/cell override changes the root profile fingerprint,
            # but not its immediate child names. Publish a scenario fallback so
            # arbitrary combinations work without a Python layout server.
            source = layout.get_root_layout_for_component(session.layout_key, session.root)
            fallback = layout.root_layouts.setdefault(scenario, {})
            for key, value in source.items():
                if key != "children":
                    fallback.setdefault(key, deepcopy(value))
            children = fallback.setdefault("children", {})
            for child in session.root.get_children():
                name = child.get_name()
                children.setdefault(name, deepcopy(source["children"][name]))
            layout.is_dirty = True
            layout.save_if_dirty()
        print(f"{scenario}: {fidelity} layout defaults generated")
