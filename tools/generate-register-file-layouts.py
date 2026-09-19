"""Refresh only FF register-file defaults; keep unrelated and user-styled entries."""
from copy import deepcopy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "visualizer-v2"))
import server
import register_file_layout as floorplan


def walk(component):
    yield component
    for child in component.get_children():
        yield from walk(child)


manager = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
old_types = deepcopy(manager.type_layouts)
test = server.circuit_backend.create_test_by_name("RegisterFile32x32Test")
test.setup_circuit()
root = test.get_root()


def update(entry, style, children=None, previous=None):
    if entry.get("layout_revision") != floorplan.REVISION:
        entry["previous_default"] = {k:deepcopy(v) for k,v in (previous or entry).items()
                                     if k not in {"previous_default", "layout_revision", "color"}}
    entry.update(style)
    if children is not None:
        for name, geometry in children.items():
            entry.setdefault("children", {}).setdefault(name, {}).update(geometry)
    entry["layout_revision"] = floorplan.REVISION


for store in [manager.type_layouts, *manager.profile_layouts.values()]:
    for kind, entry in store.items():
        style = floorplan.defaults(kind)
        if style:
            update(entry, style, previous={**manager.settings, **old_types.get(kind,{}), **entry})
# Seed all new child type proportions before sizing any parent slots. Otherwise
# the first run fits the bank to its old pin-count minimum, then shrinks its
# aspect later, leaving an unnecessarily tiny bank in the register-file view.
for component in walk(root):
    kind = server._component_layout_type(component)
    style = floorplan.defaults(kind)
    if style:
        update(manager.type_layouts.setdefault(kind, {}), style)
    manager.ensure_component_layout_defaults(component)
seen = set()
for component in walk(root):
    kind = server._component_layout_type(component)
    # Populate newly visible 32-bit word/cell descendants too.
    manager.ensure_component_layout_defaults(component)
    if kind in seen:
        continue
    seen.add(kind)
    children = list(component.get_children())
    aspect = manager.aspect_for_component(component)
    placements = server._layered_graph_layout(component, children, aspect,
        min(.7,manager.visual_ratio_for_component(component,"title_bar_ratio")/aspect),
        {c.get_id():manager.aspect_for_component(c) for c in children},
        manager.visual_ratio_for_component(component,"boundary_area_ratio"),
        manager.visual_ratio_for_component(component,"pin_size_ratio"), manager.child_stub_ratios(children))
    style = floorplan.defaults(kind)
    if style:
        update(manager.type_layouts[kind], style, placements)
        for profile in manager.profile_layouts.values():
            if kind in profile:
                update(profile[kind], style, placements)
    else:
        for name, geometry in placements.items():
            manager.type_layouts[kind].setdefault("children",{}).setdefault(name, geometry)
for alias in ("rv32i-register-file", "register-file32x32", "register-file-32x32", "registerfile32x32", "rf32x32"):
    for key in {alias, *(k for k in manager.root_layouts if k.startswith(alias+"@"))}:
        entry = manager.root_layouts.setdefault(key,{})
        update(entry,floorplan.defaults("RegisterFile32x32"),manager.type_layouts["RegisterFile32x32"]["children"],
               previous={**manager.settings, **old_types.get("RegisterFile32x32",{}), **entry})
manager.is_dirty = True
manager.save_if_dirty()
print("Register file, 31-word bank, and byte-organized 32-bit words refreshed.")
