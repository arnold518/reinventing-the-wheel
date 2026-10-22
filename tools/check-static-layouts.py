"""List children the static (WebAssembly) visualizer cannot place, layout keys
shared by parts of different internal structure, squeezed SRAM floorplans, and
children placed outside their parent's content area: over the title bar
(`title_bar_ratio` of the parent's width, as `contentRect` in app.js draws
it), in the side margins where the pins are drawn, or past the bottom.

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
import sram_layout

ROOT = Path(__file__).resolve().parents[1]


def merge(base: dict, override: dict) -> dict:
    return {**base, **override, "children": {**base.get("children", {}), **override.get("children", {})}}


def type_layout(layout: dict, component) -> dict:
    types = layout.get("type_layouts", {})
    kind = server._component_type(component)
    key = server._component_layout_type(component)
    base = dict(types.get(kind, {}))
    if server._structure_keyed(kind, key):
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


# Layout key -> (children of the first part seen with it, where). One saved
# floorplan cannot fit two different internal structures (an 8-row and a
# 16-row SRAM, a one-bit and a two-bit predictor entry); such a type needs a
# structure key (see _component_layout_type in visualizer-v2/server.py).
STRUCTURES: dict[str, tuple[tuple, str]] = {}


def scope_of(component) -> str:
    fingerprint = server._component_profile_fingerprint(component)
    key = server._component_layout_type(component)
    return f"profile_layouts.{fingerprint}.{key}" if fingerprint not in ("", "explicit") else f"type_layouts.{key}"


MANAGER = server.LayoutManager(ROOT / "visualizer-v2/layout.json")
ASPECTS: dict[tuple, float] = {}


def aspect_of(component) -> float:
    key = (server._component_layout_type(component), server._component_profile_fingerprint(component))
    if key not in ASPECTS:
        ASPECTS[key] = MANAGER.aspect_for_component(component)
    return ASPECTS[key]


def outside_content(component, entry: dict, children, root_key: str | None) -> list[str]:
    """Children not inside the parent's content area (see the module text)."""
    kwargs = dict(scenario_key=root_key, is_root=True) if root_key else {}
    aspect = MANAGER.aspect_for_component(component, **kwargs) if root_key else aspect_of(component)
    title = MANAGER.visual_ratio_for_component(component, "title_bar_ratio", **kwargs) / aspect
    margin = MANAGER.visual_ratio_for_component(component, "boundary_area_ratio", **kwargs)
    found = []
    for child in children:
        placement = entry.get("children", {}).get(child.get_name(), {})
        if not placed(placement):
            continue
        x, y = (float(v) for v in placement["rel_pos"][:2])
        width = float(placement["rel_width"])
        height = width * aspect_of(child) / aspect
        if y < title - 1e-6 or y + height > 1 + 1e-6 or x < margin - 1e-6 or x + width > 1 - margin + 1e-6:
            found.append(f"{child.get_name()} (x {x:.3f}..{x + width:.3f}, y {y:.3f}..{y + height:.3f}; "
                         f"content x {margin:.3f}..{1 - margin:.3f}, y {title:.3f}..1)")
    return found


def narrow(entry: dict, children) -> bool:
    """A matrix floorplan squeezed into a thin strip (placed before its
    children's proportions were known)."""
    spans = [(p["rel_pos"][0], p["rel_pos"][0] + p["rel_width"])
             for c in children if placed(p := entry.get("children", {}).get(c.get_name(), {}))]
    return len(spans) >= 3 and max(b for _, b in spans) - min(a for a, _ in spans) < 0.35


def missing_in(layout: dict, alias: str, class_name: str) -> list[str]:
    root = circuit_backend.create_layout_topology(class_name)
    if root is None:
        return [f"{alias}: no topology"]
    root_key = server.scenario_layout_key(alias, root)
    problems = []

    def visit(component, depth, path=""):
        children = list(component.get_children())
        if children and depth > 0:
            scope = scope_of(component)
            shape = tuple(sorted((c.get_name(), server._component_type(c)) for c in children))
            first = STRUCTURES.setdefault(scope, (shape, f"{alias}:{path}"))
            if first[0] != shape:
                problems.append(f"{alias}: SHARED {scope} at {path} differs from {first[1]}")
            if sram_layout.defaults(server._component_type(component)) and narrow(type_layout(layout, component), children):
                problems.append(f"{alias}: NARROW {scope} at {path}")
        if children:
            entry = type_layout(layout, component)
            if depth == 0:
                entry = merge(entry, layout.get("root_layouts", {}).get(root_key, {}))
            where = f"root_layouts.{root_key}" if depth == 0 else scope_of(component)
            for child in outside_content(component, entry, children, root_key if depth == 0 else None):
                problems.append(f"{alias}: OUTSIDE {where}: {child}")
            for child in children:
                if not placed(entry.get("children", {}).get(child.get_name(), {})):
                    scope = (f"root_layouts.{root_key}" if depth == 0 else
                             f"profile_layouts.{server._component_profile_fingerprint(component)}."
                             f"{server._component_layout_type(component)}"
                             if server._component_profile_fingerprint(component) not in ("", "explicit") else
                             f"type_layouts.{server._component_layout_type(component)}")
                    problems.append(f"{alias}: {scope}.children.{child.get_name()}")
        for child in children:
            visit(child, depth + 1, f"{path}/{child.get_name()}")

    visit(root, 0)
    del root
    gc.collect()
    return problems


def main() -> int:
    layout = json.loads((ROOT / "visualizer-v2/layout.json").read_text(encoding="utf-8"))
    wanted = set(sys.argv[1:])
    unknown = wanted - {option["key"] for option in server.SCENARIO_OPTIONS}
    if unknown:
        raise ValueError("Unknown scenario aliases: " + ", ".join(sorted(unknown)))
    options = [o for o in server.SCENARIO_OPTIONS if not wanted or o["key"] in wanted]
    problems = []
    for index, option in enumerate(options, 1):
        found = missing_in(layout, option["key"], option["className"])
        print(f"[{index}/{len(options)}] {option['key']}: {len(found)} missing", flush=True)
        problems += found
    for line in sorted(set(problems)):
        print("PROBLEM", line)
    print(f"{len(set(problems))} layout problems in {len(options)} scenarios")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
