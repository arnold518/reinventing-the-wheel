#!/usr/bin/env python3
"""Refresh only floorplans changed by balanced decode and branch prediction.

Uses topology, not simulation. Explicit manual layouts and unrelated entries
are untouched; old geometry is retained for browser default migration.
Set PYTHONPATH to the desired native build before running this tool.
"""
import argparse
from copy import deepcopy
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT / "visualizer-v2"))
import server
import recalculate_layouts as layouts

KINDS = {"RV32IBitPatternMatcher", "RV32IExecuteStage",
         "RV32IExecutePacketStatus", "RV32IPredictionPath"}


def refresh(path):
    manager = server.LayoutManager(path)
    representatives = {}
    roots = {}
    for test in ("RV32IBitPatternMatcherTest", "RV32IExecuteStageTest", "RV32IPredictionPathTest"):
        root = server.circuit_backend.create_layout_topology(test)
        roots[server.scenario_canonical_alias(test)] = root
        for component, _ in layouts._walk(root):
            if component.get_type_name() in KINDS:
                representatives[component.get_type_name()] = component
    # Old saved mask/value variants remain valid even if no current decoder
    # instantiates them. Build each exact mask rather than borrow another shape.
    def resolve(kind):
        match = re.fullmatch(r"RV32IBitPatternMatcher<mask=(0x[\da-f]+),value=(0x[\da-f]+)>", kind)
        if match:
            return server.circuit_backend.RV32IBitPatternMatcher("MATCHER", int(match[1], 16), int(match[2], 16))
        return representatives.get(kind)

    changed, manual = [], []
    def update(scope, entry, component, fingerprint="explicit", alias=None):
        view = layouts.ProfileLayoutView(manager, fingerprint)
        effective = (view.get_root_layout_for_component(alias, component) if alias
                     else view.get_layout_for_component(component))
        if entry.get("layout_mode") == "manual" or effective.get("layout_mode") == "manual":
            manual.append(scope)
            return
        children = list(component.get_children())
        if not children:  # Behavioral profile: never substitute structural internals.
            return
        generated = server._layered_graph_layout(*layouts.layout_arguments(view, component, children, alias))
        layouts._assert_child_bounds(view, component, children, generated, alias)
        layouts._assert_no_child_overlap(view, component, children, generated, 0 if alias else 1, alias or "")
        before = deepcopy(entry)
        layouts.refresh_entry(entry, generated)
        # Retired AND-chain children survive in automatic_layout_history only.
        entry["children"] = {name: entry["children"][name] for name in generated}
        entry["layout_mode"] = "automatic" if component.get_type_name() == "RV32IExecutePacketStatus" else "floorplan"
        if entry != before:
            changed.append(scope)

    for fingerprint, store in [("explicit", manager.type_layouts), *manager.profile_layouts.items()]:
        for kind, entry in store.items():
            if kind.split("<")[0] not in KINDS:
                continue
            # A compact saved profile has no child geometry to refresh.
            if fingerprint != "explicit" and not entry.get("children"):
                continue
            component = resolve(kind)
            update(f"{fingerprint}/{kind}", entry, component, fingerprint)
    for alias, entry in manager.root_layouts.items():
        base, _, fingerprint = alias.partition("@")
        if base in roots:
            update(f"root/{alias}", entry, roots[base], fingerprint or "explicit", alias)
    if changed:
        manager.save()
    return {"changed": changed, "preserved_manual": manual}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path, default=ROOT / "visualizer-v2/layout.json")
    result = refresh(parser.parse_args().layout)
    print(f"Updated {len(result['changed'])} scopes; preserved {len(result['preserved_manual'])} manual scopes")
