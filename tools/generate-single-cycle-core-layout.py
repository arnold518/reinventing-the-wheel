"""Refresh the complete single-cycle core only; keep all stage interiors intact.

Use construction-only topologies, not simulation/history. Recenter the smaller
core in existing program fixtures without moving either memory or the clock.
"""
from copy import deepcopy
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import rv32i_layout
import recalculate_layouts as refresh


def refresh_layout(path):
    manager = server.LayoutManager(path)
    before = deepcopy(manager.to_jsonable())
    core = server.circuit_backend.create_layout_topology("RV32ISingleCycleCoreTest")
    kind = "RV32ISingleCycleCore"
    entries = [(manager.type_layouts[kind], "explicit", None)]
    entries.extend((types[kind], fingerprint, None)
                   for fingerprint, types in manager.profile_layouts.items() if kind in types)
    keys = {"rv32i-core", server.scenario_layout_key("rv32i-core", core)}
    keys.update(k for k in manager.root_layouts if k.split("@")[0] == "rv32i-core")
    entries.extend((manager.root_layouts.setdefault(k, {}), k.split("@", 1)[1] if "@" in k else "explicit", k)
                   for k in sorted(keys))
    old_type = deepcopy(manager.type_layouts[kind])
    for entry, fingerprint, key in entries:
        if entry.get("layout_mode") == "manual":
            continue
        if entry.get("layout_revision") != rv32i_layout.SINGLE_CYCLE_REVISION:
            previous = {**deepcopy(old_type), **deepcopy(entry)}
            entry["previous_default"] = {k: v for k, v in previous.items()
                                         if k not in {"previous_default", "automatic_layout_history"}}
            # Preserve explicitly customized proportions, if any.
            if entry.get("aspect_ratio", 1.0) == 1.0:
                entry["aspect_ratio"] = rv32i_layout.ASPECTS[kind]
        view = refresh.ProfileLayoutView(manager, fingerprint)
        children = list(core.get_children())
        generated = server._custom_child_layout(*refresh.layout_arguments(view, core, children, key))
        assert generated is not None
        refresh._assert_child_bounds(view, core, children, generated, key)
        refresh._assert_no_child_overlap(view, core, children, generated, 0 if key else 1, key or "")
        refresh.refresh_entry(entry, generated)
        entry["layout_revision"] = rv32i_layout.SINGLE_CYCLE_REVISION
        entry["layout_mode"] = "floorplan"

    program = server.circuit_backend.create_layout_topology("RV32ISingleCycleSystemTest/program-03")
    children = list(program.get_children())
    for key, entry in manager.root_layouts.items():
        if not re.fullmatch(r"rv32i-program\d+(?:@[^@]+)?", key) or entry.get("layout_mode") == "manual":
            continue
        fingerprint = key.split("@", 1)[1] if "@" in key else server._component_profile_fingerprint(program)
        view = refresh.ProfileLayoutView(manager, fingerprint)
        generated = server._custom_child_layout(*refresh.layout_arguments(view, program, children, key))
        assert generated is not None
        placements = {name: deepcopy(entry.get("children", {}).get(name, placement))
                      for name, placement in generated.items()}
        placements["CORE"].update(generated["CORE"])
        refresh._assert_child_bounds(view, program, children, placements, key)
        refresh._assert_no_child_overlap(view, program, children, placements, 0, key)
        refresh.refresh_entry(entry, placements)
        entry["layout_mode"] = "floorplan"

    if before != manager.to_jsonable():
        manager.save()
    return manager.to_jsonable()


if __name__ == "__main__":
    refresh_layout(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "visualizer-v2/layout.json")
    print("Single-cycle core: stage order refreshed; stage interiors and other floorplans retained")
