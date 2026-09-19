#!/usr/bin/env python3
"""Rebuild deterministic default layouts for every visualizer scenario."""

from __future__ import annotations

import argparse
from collections import OrderedDict
import copy
import gc
import hashlib
import json
from pathlib import Path
import re
from typing import Any, Iterator

import server

AUTOMATIC_REVISION = "automatic-stub-clearance-v1"


PROGRAM_SCENARIO = re.compile(
    r"^(RV32ISingleCycleSystemTest|RV32IFiveStageCoreProgramTest)"
    r"/program-[0-9]{2}$"
)


def _scenario_group(class_name: str) -> str:
    match = PROGRAM_SCENARIO.match(class_name)
    if match:
        return match.group(1)
    return class_name


def _walk(component: Any, depth: int = 0) -> Iterator[tuple[Any, int]]:
    yield component, depth
    for child in component.get_children():
        yield from _walk(child, depth + 1)


class ProfileLayoutView(server.LayoutManager):
    """A read-only style view over the same stores, including child overrides."""
    def __init__(self, manager: server.LayoutManager, fingerprint: str):
        self.__dict__.update(manager.__dict__)
        self.fingerprint = fingerprint

    def get_layout_for_component(self, component: Any) -> dict[str, Any]:
        return self.layout_for_component_profile(component, self.fingerprint)


def layout_arguments(manager, component, children, root_key=None):
    aspect = manager.aspect_for_component(component, scenario_key=root_key, is_root=root_key is not None)
    def ratio(key):
        return manager.visual_ratio_for_component(component, key, scenario_key=root_key, is_root=root_key is not None)
    return (component, children, aspect, min(.7, ratio("title_bar_ratio") / max(aspect, .01)),
            {child.get_id(): manager.aspect_for_component(child) for child in children},
            ratio("boundary_area_ratio"), ratio("pin_size_ratio"), manager.child_stub_ratios(children))


def refresh_entry(entry, placements):
    """Replace geometry only, preserving styles, metadata and migration history."""
    old = copy.deepcopy(entry.get("children", {}))
    digest = hashlib.sha256(json.dumps(placements, sort_keys=True).encode()).hexdigest()[:12]
    revision = f"{AUTOMATIC_REVISION}:{digest}"
    if entry.get("automatic_layout_revision") != revision:
        entry.setdefault("automatic_layout_history", []).append({
            "revision": entry.get("automatic_layout_revision"), "children": old,
        })
    for name, placement in placements.items():
        entry.setdefault("children", {}).setdefault(name, {}).update(copy.deepcopy(placement))
    entry["automatic_layout_revision"] = revision
    entry["layout_mode"] = "automatic"
    return old != entry["children"]


def refresh_automatic(layout_path: Path, scenarios: list[str] | None = None, *, dry_run=False):
    """Preserve floorplans/styles; refresh only known, matching automatic scopes.

    No CircuitSession or simulation/history run. One representative test topology
    is kept alive at a time. Unknown/retired saved topologies are reported, not
    guessed from a different component's children.
    """
    manager = server.LayoutManager(layout_path)
    original = copy.deepcopy(manager.to_jsonable())
    groups = OrderedDict()
    selected = {server.SCENARIO_ALIASES_TO_CANONICAL[s] for s in scenarios} if scenarios else None
    for option in server.SCENARIO_OPTIONS:
        if selected is None or option["key"] in selected:
            groups.setdefault(_scenario_group(option["className"]), []).append(option)
    report = {"automatic": [], "changed": [], "preserved_custom": [], "preserved_manual": [],
              "unmatched": [], "representatives": len(groups), "component_count": 0}
    visited, considered = set(), set()

    def update(scope, entry, component, children, fingerprint, root_key=None):
        considered.add(scope)
        if scope in visited:
            return
        view = ProfileLayoutView(manager, fingerprint)
        effective = (view.get_root_layout_for_component(root_key, component) if root_key
                     else view.get_layout_for_component(component))
        if entry.get("layout_mode") == "manual" or effective.get("layout_mode") == "manual":
            visited.add(scope)
            report["preserved_manual"].append(scope)
            return
        # A profile's old/retired topology must not be silently replaced using
        # another profile's representative. Wait for a matching topology.
        names = {child.get_name() for child in children}
        if set(entry.get("children", {})) - names:
            return
        arguments = layout_arguments(view, component, children, root_key)
        if server._custom_child_layout(*arguments) is not None:
            visited.add(scope)
            report["preserved_custom"].append(scope)
            return
        generated = server._layered_graph_layout(*arguments)
        _assert_no_child_overlap(view, component, children, generated, 0 if root_key else 1, root_key or "")
        _assert_child_bounds(view, component, children, generated, root_key)
        visited.add(scope)
        if refresh_entry(entry, generated):
            report["changed"].append(scope)
        report["automatic"].append(scope)

    for group_index, options in enumerate(groups.values(), 1):
        option = options[0]
        print(f"[{group_index}/{len(groups)}] topology only: {option['className']}", flush=True)
        root = server.circuit_backend.create_layout_topology(option["className"])
        if root is None:
            raise RuntimeError(f"No topology for {option['className']}")
        components = list(_walk(root))
        report["component_count"] += len(components)
        # Children first. Existing proportions are never reset or re-derived.
        for component, depth in reversed(components):
            kind = server._component_layout_type(component)
            if kind not in manager.type_layouts:
                manager.ensure_component_layout_defaults(component)
            children = list(component.get_children())
            if not children:
                continue
            if kind != "Component":  # Generic test wrappers are scenario-specific.
                update(f"type/{kind}", manager.type_layouts[kind], component, children, "explicit")
                for fingerprint, entries in manager.profile_layouts.items():
                    if kind in entries and entries[kind].get("children"):
                        update(f"profile/{fingerprint}/{kind}", entries[kind], component, children, fingerprint)
            if depth == 0:
                canonical = {item["key"] for item in options}
                root_keys = [key for key in manager.root_layouts
                    if server.SCENARIO_ALIASES_TO_CANONICAL.get(key.split("@")[0], key.split("@")[0]) in canonical]
                for key in root_keys:
                    entry = manager.root_layouts[key]
                    if entry.get("children"):
                        fingerprint = key.split("@", 1)[1] if "@" in key else server._component_profile_fingerprint(root)
                        update(f"root/{key}", entry, root, children, fingerprint, key)
        del component, children, components, root
        gc.collect()

    report["unmatched"] = sorted(considered - visited)
    stored_scopes = {f"type/{kind}" for kind, entry in original["type_layouts"].items() if entry.get("children")}
    stored_scopes.update(f"root/{key}" for key, entry in original["root_layouts"].items() if entry.get("children"))
    stored_scopes.update(f"profile/{fingerprint}/{kind}"
        for fingerprint, entries in original["profile_layouts"].items()
        for kind, entry in entries.items() if entry.get("children"))
    report["unvisited"] = sorted(stored_scopes - considered)
    for key in ("automatic", "changed", "preserved_custom", "preserved_manual"):
        report[key].sort()
    if not dry_run and manager.to_jsonable() != original:
        manager.save()
    return report


def _assert_child_bounds(manager, component, children, placements, root_key=None):
    _, _, aspect, title, _, boundary, pin_size, stubs = layout_arguments(manager, component, children, root_key)
    margin = server._stub_margin_ratio(boundary, pin_size)
    for child in children:
        placement = placements[child.get_name()]
        x, y = placement["rel_pos"]
        width = placement["rel_width"]
        height = width * manager.aspect_for_component(child) / max(aspect, .01)
        stub = width * stubs[child.get_id()]
        pin_half_height = width * manager.visual_ratio_for_component(child, "pin_size_ratio") / (2 * max(aspect, .01))
        if not (width > 0 and x-stub >= margin-1e-9 and x+width+stub <= 1-margin+1e-9
                and y-pin_half_height >= title-1e-9 and y+height+pin_half_height <= 1+1e-9):
            raise RuntimeError(f"Child outside parent/stub bounds: {component.get_type_name()}.{child.get_name()}")


def _preserve_color(
    component: Any,
    destination: dict[str, Any],
    old_type_layouts: dict[str, dict[str, Any]],
) -> None:
    layout_type = server._component_layout_type(component)
    if "color" in destination:
        return
    for candidate in (layout_type, server._component_type(component)):
        color = old_type_layouts.get(candidate, {}).get("color")
        if color is not None:
            destination["color"] = copy.deepcopy(color)
            return


def _default_child_layouts(
    manager: server.LayoutManager,
    component: Any,
    children: list[Any],
    depth: int,
    root_layout_key: str,
) -> dict[str, dict[str, Any]]:
    is_root = depth == 0
    parent_aspect = manager.aspect_for_component(
        component,
        scenario_key=root_layout_key,
        is_root=is_root,
    )
    title_ratio = manager.visual_ratio_for_component(
        component,
        "title_bar_ratio",
        scenario_key=root_layout_key,
        is_root=is_root,
    )
    title_fraction = min(0.7, title_ratio / max(parent_aspect, 0.01))
    boundary_fraction = manager.visual_ratio_for_component(
        component,
        "boundary_area_ratio",
        scenario_key=root_layout_key,
        is_root=is_root,
    )
    pin_size_fraction = manager.visual_ratio_for_component(
        component,
        "pin_size_ratio",
        scenario_key=root_layout_key,
        is_root=is_root,
    )
    child_aspects = {
        child.get_id(): manager.aspect_for_component(child)
        for child in children
    }
    return server._layered_graph_layout(
        component,
        children,
        parent_aspect,
        title_fraction,
        child_aspects,
        boundary_fraction,
        pin_size_fraction,
        manager.child_stub_ratios(children),
    )


def _assert_no_child_overlap(
    manager: server.LayoutManager,
    component: Any,
    children: list[Any],
    placements: dict[str, dict[str, Any]],
    depth: int,
    root_layout_key: str,
) -> None:
    parent_aspect = manager.aspect_for_component(
        component,
        scenario_key=root_layout_key,
        is_root=depth == 0,
    )
    rectangles: list[tuple[str, float, float, float, float]] = []
    for child in children:
        child_boundary = manager.visual_ratio_for_component(child, "boundary_area_ratio")
        child_pin_size = manager.visual_ratio_for_component(child, "pin_size_ratio")
        child_stub = server._stub_margin_ratio(child_boundary, child_pin_size)
        placement = placements[child.get_name()]
        x, y = placement["rel_pos"]
        width = placement["rel_width"]
        child_aspect = manager.aspect_for_component(child)
        height = width * child_aspect / max(parent_aspect, 0.01)
        rectangles.append(
            (
                child.get_name(),
                x - width * child_stub,
                y - width * child_pin_size / (2 * max(parent_aspect, .01)),
                width * (1 + 2 * child_stub),
                height + width * child_pin_size / max(parent_aspect, .01),
            )
        )

    for index, left in enumerate(rectangles):
        for right in rectangles[index + 1:]:
            overlap_x = min(left[1] + left[3], right[1] + right[3]) - max(left[1], right[1])
            overlap_y = min(left[2] + left[4], right[2] + right[4]) - max(left[2], right[2])
            if overlap_x > 1e-12 and overlap_y > 1e-12:
                raise RuntimeError(
                    f"default children overlap in {component.get_name()}: {left[0]} and {right[0]}"
                )


def _same_placement(left: dict[str, Any], right: dict[str, Any]) -> bool:
    return left.get("rel_pos") == right.get("rel_pos") and left.get("rel_width") == right.get("rel_width")


def _store_child_layouts(
    manager: server.LayoutManager,
    component: Any,
    placements: dict[str, dict[str, Any]],
    depth: int,
    root_layout_key: str,
) -> None:
    if depth == 0:
        manager.set_child_layouts(
            component,
            placements,
            scenario_key=root_layout_key,
            is_root=True,
        )
        return

    layout_type = server._component_layout_type(component)
    profile_fingerprint = server._component_profile_fingerprint(component)
    if profile_fingerprint == "explicit":
        manager.set_child_layouts(component, placements)
        return

    base_children = manager.type_layouts.setdefault(layout_type, {}).setdefault("children", {})
    if not base_children:
        for child_name, placement in placements.items():
            manager.update_type_child_layout(
                layout_type,
                child_name,
                placement["rel_pos"],
                placement["rel_width"],
            )
        return

    if all(
        child_name in base_children and _same_placement(base_children[child_name], placement)
        for child_name, placement in placements.items()
    ):
        return

    for child_name, placement in placements.items():
        manager.update_profile_child_layout(
            profile_fingerprint,
            layout_type,
            child_name,
            placement["rel_pos"],
            placement["rel_width"],
        )


def recalculate(layout_path: Path) -> dict[str, int]:
    previous = server.LayoutManager(layout_path)
    manager = server.LayoutManager(layout_path)
    manager.schema_version = server.LAYOUT_SCHEMA_VERSION
    manager.settings = copy.deepcopy(previous.settings or server.DEFAULT_SETTINGS)
    manager.type_layouts = {}
    manager.profile_layouts = {}
    manager.root_layouts = {}
    manager.is_dirty = True

    groups: OrderedDict[str, list[tuple[str, str]]] = OrderedDict()
    for option in server.SCENARIO_OPTIONS:
        class_name = option["className"]
        groups.setdefault(_scenario_group(class_name), []).append((option["key"], class_name))

    component_count = 0
    parent_topology_count = 0
    calculated_parent_topologies: set[
        tuple[str, str, tuple[str, ...]]
    ] = set()
    for group_index, entries in enumerate(groups.items(), start=1):
        _, scenarios = entries
        representative_alias, representative_class = scenarios[0]
        print(
            f"[{group_index}/{len(groups)}] building {representative_class}...",
            flush=True,
        )
        scenario_type = server.SCENARIOS[representative_alias]
        test_scenario = scenario_type()
        test_scenario.setup_circuit()
        root = test_scenario.get_root()
        if root is None:
            raise RuntimeError(f"{representative_class} did not build a visual root")

        root_layout_key = server.scenario_layout_key(representative_alias, root)
        components = list(_walk(root))
        component_count += len(components)

        for component, depth in components:
            manager.ensure_component_layout_defaults(
                component,
                scenario_key=(
                    root_layout_key if depth == 0 else None
                ),
                is_root=depth == 0,
            )
            type_entry = manager.type_layouts[server._component_layout_type(component)]
            _preserve_color(component, type_entry, previous.type_layouts)

        root_layout = manager.root_layouts.setdefault(root_layout_key, {})
        root_layout["min_aspect_ratio"] = manager.effective_minimum_aspect_for_component(
            root,
            scenario_key=root_layout_key,
            is_root=True,
        )
        root_layout["aspect_ratio"] = manager.aspect_for_component(
            root,
            scenario_key=root_layout_key,
            is_root=True,
        )
        for component, depth in components:
            children = list(component.get_children())
            if not children:
                continue
            topology_key = (
                server._component_layout_type(component),
                server._component_profile_fingerprint(component),
                tuple(sorted(
                    child.get_name() for child in children
                )),
            )
            if depth != 0 and topology_key in calculated_parent_topologies:
                continue
            placements = _default_child_layouts(
                manager,
                component,
                children,
                depth,
                root_layout_key,
            )
            _assert_no_child_overlap(
                manager,
                component,
                children,
                placements,
                depth,
                root_layout_key,
            )
            _store_child_layouts(
                manager,
                component,
                placements,
                depth,
                root_layout_key,
            )
            if depth != 0:
                calculated_parent_topologies.add(topology_key)
            parent_topology_count += 1

        representative_root_layout = copy.deepcopy(manager.root_layouts[root_layout_key])
        for scenario_alias, _ in scenarios:
            target_key = server.scenario_layout_key(scenario_alias, root)
            root_layout = copy.deepcopy(representative_root_layout)
            old_root = previous.root_layouts.get(target_key, {})
            if "color" in old_root:
                root_layout["color"] = copy.deepcopy(old_root["color"])
            manager.root_layouts[target_key] = root_layout

        print(
            f"[{group_index}/{len(groups)}] completed {representative_class}: "
            f"{len(components)} components, {len(scenarios)} scenario layout(s)",
            flush=True,
        )
        del root, test_scenario, components
        gc.collect()

    manager.save()
    return {
        "scenario_count": len(server.SCENARIO_OPTIONS),
        "representative_count": len(groups),
        "component_count": component_count,
        "parent_topology_count": parent_topology_count,
        "type_layout_count": len(manager.type_layouts),
        "profile_count": len(manager.profile_layouts),
        "profile_layout_count": sum(len(layouts) for layouts in manager.profile_layouts.values()),
        "root_layout_count": len(manager.root_layouts),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--layout",
        type=Path,
        default=server.LAYOUT_PATH,
        help="layout JSON to replace (default: visualizer-v2/layout.json)",
    )
    parser.add_argument(
        "--fill-missing", nargs="+", metavar="SCENARIO",
        help="only add missing defaults for these scenarios, preserving saved placements",
    )
    parser.add_argument(
        "--reflow-types", nargs="+", default=[], metavar="TYPE",
        help="with --fill-missing, also replace child placements of only these component types",
    )
    parser.add_argument("--automatic-only", action="store_true",
                        help="Refresh automatic geometry while preserving floorplans, styles and manual entries")
    parser.add_argument("--scenarios", nargs="+", help="Limit --automatic-only to these scenario aliases")
    parser.add_argument("--dry-run", action="store_true", help="Validate and report without saving (--automatic-only)")
    parser.add_argument("--report", type=Path, help="Write the selective refresh inventory as JSON")
    args = parser.parse_args()
    if args.automatic_only:
        if args.fill_missing or args.reflow_types:
            parser.error("--automatic-only cannot be combined with legacy reset/fill modes")
        if args.scenarios and any(s not in server.SCENARIOS for s in args.scenarios):
            parser.error("Unknown scenario alias")
        report = refresh_automatic(args.layout.resolve(), args.scenarios, dry_run=args.dry_run)
        if args.report:
            args.report.write_text(json.dumps(report, indent=2) + "\n")
        for key, value in report.items():
            print(f"{key}: {len(value) if isinstance(value, list) else value}")
        return
    if args.scenarios or args.dry_run or args.report:
        parser.error("--scenarios, --dry-run and --report require --automatic-only")
    if args.reflow_types and not args.fill_missing:
        parser.error("--reflow-types requires --fill-missing")
    if args.fill_missing:
        manager = server.LayoutManager(args.layout.resolve())
        for scenario in args.fill_missing:
            if scenario not in server.SCENARIOS:
                parser.error(f"Unknown scenario: {scenario}")
        for scenario in args.fill_missing:
            session = server.CircuitSession(scenario, manager)
            root_key = server.scenario_layout_key(scenario, session.root)
            for component, depth in _walk(session.root):
                if server._component_type(component) not in args.reflow_types:
                    continue
                children = list(component.get_children())
                if not children:
                    continue
                placements = _default_child_layouts(manager, component, children, depth, root_key)
                _assert_no_child_overlap(manager, component, children, placements, depth, root_key)
                layout_type = server._component_layout_type(component)
                # The child topology is the same in nested/profile-selected ALUs.
                # Preserve unrelated types and the ALU's placement in its parent.
                manager.type_layouts[layout_type]["children"] = copy.deepcopy(placements)
                for profile in manager.profile_layouts.values():
                    if layout_type in profile:
                        profile[layout_type]["children"] = copy.deepcopy(placements)
                if depth == 0:
                    manager.root_layouts[root_key]["children"] = copy.deepcopy(placements)
                manager.is_dirty = True
            manager.save()
            print(f"Filled missing layouts for {scenario}: {len(session.components)} components", flush=True)
            del session
            gc.collect()
        return
    summary = recalculate(args.layout.resolve())
    print("Layout regeneration complete:")
    for key, value in summary.items():
        print(f"  {key}: {value}")


if __name__ == "__main__":
    main()
