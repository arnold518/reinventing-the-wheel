#!/usr/bin/env python3
"""Rebuild deterministic default layouts for every visualizer scenario."""

from __future__ import annotations

import argparse
from collections import OrderedDict
import copy
import gc
from pathlib import Path
import re
from typing import Any, Iterator

import server


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
    boundary = manager.visual_ratio_for_component(
        component,
        "boundary_area_ratio",
        scenario_key=root_layout_key,
        is_root=depth == 0,
    )
    pin_size = manager.visual_ratio_for_component(
        component,
        "pin_size_ratio",
        scenario_key=root_layout_key,
        is_root=depth == 0,
    )
    rectangles: list[tuple[str, float, float, float, float]] = []
    for child in children:
        placement = placements[child.get_name()]
        x, y = placement["rel_pos"]
        width = placement["rel_width"]
        child_aspect = manager.aspect_for_component(child)
        height = width * child_aspect / max(parent_aspect, 0.01)
        rectangles.append(
            (
                child.get_name(),
                x - width * boundary,
                y - width * pin_size / 2,
                width * (1 + 2 * boundary),
                height + width * pin_size,
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
    args = parser.parse_args()
    summary = recalculate(args.layout.resolve())
    print("Layout regeneration complete:")
    for key, value in summary.items():
        print(f"  {key}: {value}")


if __name__ == "__main__":
    main()
