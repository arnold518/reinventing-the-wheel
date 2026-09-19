"""Refresh only the new Register32 shell; preserve every other floorplan/style."""
from copy import deepcopy
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as refresh

manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
root = server.circuit_backend.create_layout_topology("Register32Test")
children = list(root.get_children())
before = deepcopy(manager.to_jsonable())
for component, _ in refresh._walk(root):
    manager.ensure_component_layout_defaults(component)

entries = [(manager.type_layouts["Register32"], "explicit", None)]
entries += [(entries["Register32"], fingerprint, None)
            for fingerprint, entries in manager.profile_layouts.items() if "Register32" in entries]
keys = {"register32", server.scenario_layout_key("register32", root)}
keys.update(key for key in manager.root_layouts if key.split("@")[0] in ("register32", "register-32"))
entries += [(manager.root_layouts.setdefault(key, {}), key.split("@", 1)[1] if "@" in key else "explicit", key)
            for key in sorted(keys)]
for entry, fingerprint, key in entries:
    view = refresh.ProfileLayoutView(manager, fingerprint)
    arguments = refresh.layout_arguments(view, root, children, key)
    generated = server._layered_graph_layout(*arguments)
    refresh._assert_no_child_overlap(view, root, children, generated, 0 if key else 1, key or "")
    refresh._assert_child_bounds(view, root, children, generated, key)
    # refresh_entry archives prior geometry for browser migration. Remove only
    # retired direct-bit children from the active default, not arbitrary data.
    refresh.refresh_entry(entry, generated)
    for name in list(entry["children"]):
        if name in {"D_SPLIT", "Q_JOIN"} or re.fullmatch(r"BIT_(?:[0-9]|[12][0-9]|3[01])", name):
            del entry["children"][name]
if before != manager.to_jsonable():
    manager.save()
print(f"Register32: {len(entries)} type/profile/root entries refreshed; other floorplans retained")
