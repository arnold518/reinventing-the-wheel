"""Refresh decode-field and fetch-request floorplans from current topologies.

Includes the dense two-bus Rewire style and saved-default migration metadata.
Builds topology only; preserves manual scopes and unrelated geometry.
"""
import argparse
from copy import deepcopy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as layouts

REVISION = "rv32i-routing-v1"
REWIRE = "Rewire<in=2,out=40>"


def refresh(path):
    manager = server.LayoutManager(path)
    before = deepcopy(manager.to_jsonable())
    changed, manual = [], []
    old_rewire = deepcopy(manager.type_layouts.get(REWIRE, {}))
    # Publish the adapter proportions first so enclosing field slots use the
    # same aspect that both native and WASM clients will render.
    for fingerprint, store in [("type", manager.type_layouts), *manager.profile_layouts.items()]:
        if fingerprint != "type" and REWIRE not in store:
            continue
        entry = store.setdefault(REWIRE, {})
        if entry.get("layout_mode") == "manual":
            manual.append(f"{fingerprint}/{REWIRE}")
            continue
        if entry.get("layout_revision") != REVISION:
            previous = {**manager.settings, **old_rewire, **entry}
            entry["previous_default"] = {k: deepcopy(v) for k, v in previous.items()
                                         if k not in {"previous_default", "automatic_layout_history"}}
            entry.update(server.COMPONENT_LAYOUT_DEFAULTS[REWIRE])
            entry["layout_revision"] = REVISION
            changed.append(f"{fingerprint}/{REWIRE}")

    root = server.circuit_backend.create_layout_topology("RV32IFiveStageCoreProgramTest/program-01")
    representatives = {}
    for component, _ in layouts._walk(root):
        kind = server._component_layout_type(component)
        if kind.startswith("RV32IDecodeControlField<") or kind == "RV32IFetchRequest":
            representatives[kind] = component
    for kind, component in representatives.items():
        for fingerprint, store in [("explicit", manager.type_layouts), *manager.profile_layouts.items()]:
            if fingerprint != "explicit" and kind not in store:
                continue
            entry = store.setdefault(kind, {})
            view = layouts.ProfileLayoutView(manager, fingerprint)
            if entry.get("layout_mode") == "manual" or view.get_layout_for_component(component).get("layout_mode") == "manual":
                manual.append(f"{fingerprint}/{kind}")
                continue
            children = list(component.get_children())
            generated = server._custom_child_layout(*layouts.layout_arguments(view, component, children))
            if generated is None:
                raise RuntimeError(f"Missing floorplan: {kind}")
            layouts._assert_child_bounds(view, component, children, generated)
            layouts._assert_no_child_overlap(view, component, children, generated, 1, "")
            old = deepcopy(entry)
            layouts.refresh_entry(entry, generated)
            entry["children"] = {name: entry["children"][name] for name in generated}
            entry["layout_mode"] = "floorplan"
            if old != entry:
                changed.append(f"{fingerprint}/{kind}")
    if before != manager.to_jsonable():
        manager.save()
    return {"changed": changed, "preserved_manual": manual}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path, default=ROOT / "visualizer-v2/layout.json")
    result = refresh(parser.parse_args().layout)
    print(f"Updated {len(result['changed'])} routing scopes; preserved {len(result['preserved_manual'])} manual scopes")
