"""Refresh Writeback, execution status, and advanced-core floorplans.

Topology only: no simulation/history. Keep manual layouts and unrelated
placements intact; remove obsolete children only in these three scopes.
"""
from pathlib import Path
import copy
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh

manager = server.LayoutManager(server.LAYOUT_PATH)
for kind, test, alias in (
    ("RV32IWritebackStage", "RV32IWritebackStageTest", "rv32i-pipeline-writeback"),
    ("RV32IPipelineExecutionStatus", "RV32IPipelineExecutionStatusTest", "rv32i-pipeline-status"),
    ("RV32IFiveStageCore", "RV32IFiveStageCoreTest", "rv32i-five-stage-core"),
):
    canonical = server.SCENARIO_ALIASES_TO_CANONICAL.get(alias, alias)
    component = server.circuit_backend.create_layout_topology(test)
    manager.ensure_component_layout_defaults(component)
    entries = [(manager.type_layouts.setdefault(kind, {}), "explicit", None)]
    entries += [(types[kind], fingerprint, None)
                for fingerprint, types in manager.profile_layouts.items() if kind in types]
    entries += [(entry, key.split("@", 1)[1] if "@" in key else "explicit", key)
                for key, entry in manager.root_layouts.items()
                if server.SCENARIO_ALIASES_TO_CANONICAL.get(key.split("@")[0], key.split("@")[0]) == canonical]
    count = 0
    for entry, fingerprint, key in entries:
        if entry.get("layout_mode") == "manual":
            continue
        view = refresh.ProfileLayoutView(manager, fingerprint)
        effective = (view.get_root_layout_for_component(key, component) if key
                     else view.get_layout_for_component(component))
        if effective.get("layout_mode") == "manual":
            continue
        entry.update(layout_mode="floorplan", layout_revision="pipeline-stateless-writeback-v2")
        children = list(component.get_children())
        placements = server._custom_child_layout(*refresh.layout_arguments(view, component, children, key))
        assert placements is not None
        refresh._assert_child_bounds(view, component, children, placements, key)
        refresh._assert_no_child_overlap(view, component, children, placements, 0 if key else 1, key or "")
        refresh.refresh_entry(entry, placements)
        entry["children"] = {name: copy.deepcopy(entry["children"][name]) for name in placements}
        entry["layout_mode"] = "floorplan"
        count += 1
    print(f"{kind}: refreshed {count} scopes; bounds and child separation verified")
manager.save()
