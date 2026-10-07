"""Refresh only the baseline pipeline floorplan; preserve manual layouts."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh

manager = server.LayoutManager(server.LAYOUT_PATH)
core = server.circuit_backend.create_layout_topology("RV32IBaselinePipelineCoreTest")
kind = "RV32IBaselinePipelineCore"
canonical = server.SCENARIO_ALIASES_TO_CANONICAL.get("rv32i-baseline-core", "rv32i-baseline-core")
entries = [(manager.type_layouts[kind], "explicit", None)]
entries += [(types[kind], fingerprint, None) for fingerprint, types in manager.profile_layouts.items() if kind in types]
entries += [(entry, key.split("@", 1)[1] if "@" in key else "explicit", key)
            for key, entry in manager.root_layouts.items() if server.SCENARIO_ALIASES_TO_CANONICAL.get(key.split("@")[0], key.split("@")[0]) == canonical]
for entry, fingerprint, key in entries:
    if entry.get("layout_mode") == "manual":
        continue
    entry.update(aspect_ratio=.55, min_aspect_ratio=.08, pin_size_ratio=.01, layout_mode="floorplan",
                 layout_revision="rv32i-baseline-stage-order-v1")
    view = refresh.ProfileLayoutView(manager, fingerprint)
    children = list(core.get_children())
    positions = server._custom_child_layout(*refresh.layout_arguments(view, core, children, key))
    assert positions is not None
    refresh._assert_child_bounds(view, core, children, positions, key)
    refresh._assert_no_child_overlap(view, core, children, positions, 0 if key else 1, key or "")
    refresh.refresh_entry(entry, positions)
    entry["layout_mode"] = "floorplan"
manager.save()
print("Baseline pipeline: stage order, pin bounds and child separation verified")
