"""List children the static (WebAssembly) visualizer cannot place.

The native server computes a missing child placement when it builds a
session; the static site cannot, and stops with "Missing layout for …".
This replays the browser's own lookup (`getLayoutFor` / `getChildLayout` in
`visualizer-v2/static/app.js`) on `layout.json` for every component of every
visualizable scenario, building topologies only (no simulation).

Usage: python3 tools/check-static-layouts.py [SCENARIO_ALIAS ...]
Exit status 1 when a placement is missing. Run with the isolated build's
`circuit_backend` first on the path (PYTHONPATH=build-cleanup:visualizer-v2).
"""
from __future__ import annotations

import gc
import json
import sys
from pathlib import Path

import circuit_backend  # The isolated build, before server adds visualizer-v2.
import server

ROOT = Path(__file__).resolve().parents[1]


def merge(base: dict, override: dict) -> dict:
    return {**base, **override, "children": {**base.get("children", {}), **override.get("children", {})}}


def type_layout(layout: dict, component) -> dict:
    types = layout.get("type_layouts", {})
    kind = server._component_type(component)
    key = server._component_layout_type(component)
    base = dict(types.get(kind, {}))
    if kind.startswith("Cache") and key != kind:
        base.pop("children", None)
    if kind == "Rewire" and key != kind:
        base.pop("aspect_ratio", None)
        base.pop("min_aspect_ratio", None)
    specialized = base if key == kind else merge(base, types.get(key, {}))
    fingerprint = server._component_profile_fingerprint(component)
    if not fingerprint or fingerprint == "explicit":
        return specialized
    profiles = layout.get("profile_layouts", {}).get(fingerprint, {})
    if kind == "Rewire" and key != kind:
        legacy = dict(profiles.get("Rewire", {}))
        legacy.pop("aspect_ratio", None)
        legacy.pop("min_aspect_ratio", None)
        specialized = merge(specialized, legacy)
    return merge(specialized, profiles.get(key, {}))


def placed(entry) -> bool:
    try:
        x, y = (float(v) for v in entry["rel_pos"][:2])
        return float(entry["rel_width"]) == float(entry["rel_width"]) and x == x and y == y
    except (KeyError, TypeError, ValueError):
        return False


def missing_in(layout: dict, alias: str, class_name: str) -> list[str]:
    root = circuit_backend.create_layout_topology(class_name)
    if root is None:
        return [f"{alias}: no topology"]
    root_key = server.scenario_layout_key(alias, root)
    problems = []

    def visit(component, depth):
        children = list(component.get_children())
        if children:
            entry = type_layout(layout, component)
            if depth == 0:
                entry = merge(entry, layout.get("root_layouts", {}).get(root_key, {}))
            for child in children:
                if not placed(entry.get("children", {}).get(child.get_name(), {})):
                    scope = (f"root_layouts.{root_key}" if depth == 0 else
                             f"profile_layouts.{server._component_profile_fingerprint(component)}."
                             f"{server._component_layout_type(component)}"
                             if server._component_profile_fingerprint(component) not in ("", "explicit") else
                             f"type_layouts.{server._component_layout_type(component)}")
                    problems.append(f"{alias}: {scope}.children.{child.get_name()}")
        for child in children:
            visit(child, depth + 1)

    visit(root, 0)
    del root
    gc.collect()
    return problems


def main() -> int:
    layout = json.loads((ROOT / "visualizer-v2/layout.json").read_text(encoding="utf-8"))
    wanted = set(sys.argv[1:])
    options = [o for o in server.SCENARIO_OPTIONS if not wanted or o["key"] in wanted]
    problems = []
    for index, option in enumerate(options, 1):
        found = missing_in(layout, option["key"], option["className"])
        print(f"[{index}/{len(options)}] {option['key']}: {len(found)} missing", flush=True)
        problems += found
    for line in sorted(set(problems)):
        print("MISSING", line)
    print(f"{len(set(problems))} missing placements in {len(options)} scenarios")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
