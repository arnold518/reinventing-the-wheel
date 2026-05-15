#!/usr/bin/env python3
"""Browser-based CircuitSim visualizer.

This server intentionally uses only the Python standard library plus the
existing pybind11 `circuit_backend` module. The browser owns rendering and
interaction; Python owns circuit discovery, simulator state scrubbing, and
layout persistence.
"""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
import copy
import json
import mimetypes
import os
from pathlib import Path
import re
import socket
import sys
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse


ROOT_DIR = Path(__file__).resolve().parents[1]
APP_DIR = Path(__file__).resolve().parent
STATIC_DIR = APP_DIR / "static"
LAYOUT_PATH = APP_DIR / "layout.json"
BACKEND_DIR = APP_DIR

if str(BACKEND_DIR) not in sys.path:
    sys.path.insert(0, str(BACKEND_DIR))

try:
    import circuit_backend
except ImportError as exc:  # pragma: no cover - exercised manually
    raise SystemExit(
        "Could not import circuit_backend. Build the pybind11 backend first "
        "or ensure visualizer-v2/circuit_backend*.so exists."
    ) from exc


STATE_TOKENS = {
    circuit_backend.LogicValue.HIGH: "1",
    circuit_backend.LogicValue.LOW: "0",
    circuit_backend.LogicValue.HIGH_Z: "Z",
    circuit_backend.LogicValue.UNKNOWN: "X",
}

DEFAULT_COLOR = [61, 90, 128, 190]
DEFAULT_SETTINGS = {
    "title_bar_ratio": 0.15,
    "font_width_ratio": 0.1,
    "boundary_area_ratio": 0.07,
    "pin_size_ratio": 0.1,
}
MIN_VISIBLE_ASPECT_RATIO = 0.35
PIN_SPACING_CLEARANCE_RATIO = 1.15
GRID_LAYOUT_PADDING = 0.07
GRID_CHILD_FILL_RATIO = 0.95
LAYERED_MAX_ROWS_PER_COLUMN = 8
LAYERED_PADDING_X = 0.05
LAYERED_PADDING_Y = 0.055
LAYERED_HORIZONTAL_FILL_RATIO = 0.88
LAYERED_VERTICAL_FILL_RATIO = 0.9
LAYERED_DENSE_ROW_WIDTH_CAP = 0.1
GENERIC_ROOT_TYPES = {"Component", "IOComponent", "BasicComponent"}
SEQUENTIAL_TYPES = {"DFlipFlop"}
CLOCK_PIN_NAMES = {"CLK", "CLOCK"}
SEQUENTIAL_INPUT_PIN_NAMES = {"D", "RST", "RESET", "SET", "CLR", "CLEAR", "EN", "ENABLE", "LOAD"}
NON_VISUALIZABLE_TESTS = {"WireTemplateTest", "RewireValidationTest"}
PREFERRED_SCENARIO_ALIASES = {
    "FullAdderTest": ["full-adder", "fulladder"],
    "HalfAdderTest": ["half-adder", "halfadder"],
    "FullCircuitTest": ["full-circuit", "fullcircuit"],
    "DFlipFlopTest": ["dff", "d-flip-flop"],
    "ClockGeneratorTest": ["clock"],
    "Adder8Test": ["adder8", "8-bit-adder"],
    "ZeroDetect8Test": ["zero-detect8"],
    "ALU8Test": ["alu8"],
}


def _clone(value: Any) -> Any:
    return copy.deepcopy(value)


def _logic_token(value: Any) -> str:
    return STATE_TOKENS.get(value, "X")


def _signal_width(cpp_handle: Any) -> int:
    try:
        return max(1, int(cpp_handle.get_width()))
    except (AttributeError, TypeError, ValueError):
        return 1


def _signal_value(cpp_handle: Any) -> str:
    width = _signal_width(cpp_handle)
    bits: list[str] = []
    for index in range(width):
        try:
            bits.append(_logic_token(cpp_handle.get_bit(index)))
        except (AttributeError, IndexError, RuntimeError):
            if index == 0:
                bits.append(_logic_token(cpp_handle.get_value()))
            else:
                bits.append("X")
    return bits[0] if width == 1 else "".join(reversed(bits))


def _natural_key(value: str) -> list[Any]:
    return [int(part) if part.isdigit() else part.lower() for part in re.split(r"(\d+)", value)]


def _component_owner_id(pin: Any) -> str | None:
    try:
        owner = pin.get_owner()
        return owner.get_id() if owner else None
    except (AttributeError, RuntimeError):
        return None


def _component_type(component: Any) -> str:
    try:
        return component.get_type_name()
    except AttributeError:
        return "Component"


def _pin_name(pin: Any) -> str:
    try:
        return pin.get_name()
    except AttributeError:
        return ""


def _float_value(value: Any, default: float) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return default
    return result if result == result else default


def _component_pin_count(component: Any, getter_name: str) -> int:
    try:
        pins = getattr(component, getter_name)()
    except (AttributeError, RuntimeError):
        return 0
    try:
        return len(pins)
    except TypeError:
        return 0


def _minimum_aspect_ratio_for_pins(component: Any, settings: dict[str, Any]) -> float:
    title_ratio = max(0.0, _float_value(settings.get("title_bar_ratio"), DEFAULT_SETTINGS["title_bar_ratio"]))
    pin_size_ratio = max(0.0, _float_value(settings.get("pin_size_ratio"), DEFAULT_SETTINGS["pin_size_ratio"]))
    pin_count = max(
        _component_pin_count(component, "get_input_pins"),
        _component_pin_count(component, "get_output_pins"),
    )
    if pin_count == 0:
        minimum = max(MIN_VISIBLE_ASPECT_RATIO, title_ratio + pin_size_ratio)
    else:
        minimum = max(
            MIN_VISIBLE_ASPECT_RATIO,
            title_ratio + pin_size_ratio * PIN_SPACING_CLEARANCE_RATIO * (pin_count + 1),
        )
    return round(minimum, 3)


def _tarjan_scc(nodes: list[str], edges: list[tuple[str, str, str]]) -> list[list[str]]:
    adjacency: dict[str, list[str]] = {node: [] for node in nodes}
    for src, dst, _sink_pin in edges:
        if src in adjacency and dst in adjacency:
            adjacency[src].append(dst)

    index = 0
    stack: list[str] = []
    on_stack: set[str] = set()
    indices: dict[str, int] = {}
    lowlinks: dict[str, int] = {}
    result: list[list[str]] = []

    def strongconnect(node: str) -> None:
        nonlocal index
        indices[node] = index
        lowlinks[node] = index
        index += 1
        stack.append(node)
        on_stack.add(node)

        for neighbor in adjacency[node]:
            if neighbor not in indices:
                strongconnect(neighbor)
                lowlinks[node] = min(lowlinks[node], lowlinks[neighbor])
            elif neighbor in on_stack:
                lowlinks[node] = min(lowlinks[node], indices[neighbor])

        if lowlinks[node] == indices[node]:
            component: list[str] = []
            while True:
                member = stack.pop()
                on_stack.remove(member)
                component.append(member)
                if member == node:
                    break
            result.append(component)

    for node in nodes:
        if node not in indices:
            strongconnect(node)
    return result


def _stub_margin_ratio(boundary_fraction: float, pin_size_fraction: float) -> float:
    return max(0.0, boundary_fraction, pin_size_fraction / 2)


def _max_width_with_stubs(boundary_fraction: float, stub_ratio: float) -> float:
    return max(0.0, (1 - 2 * boundary_fraction) / max(1 + 2 * stub_ratio, 0.01))


def _max_column_width_with_stubs(column_count: int, boundary_fraction: float, stub_ratio: float) -> float:
    if column_count <= 1:
        return _max_width_with_stubs(boundary_fraction, stub_ratio)
    return max(0.0, (1 - 2 * boundary_fraction) / max(column_count * (1 + 2 * stub_ratio), 0.01))


def _constrain_child_horizontal(
    rel_left: float,
    rel_width: float,
    boundary_fraction: float,
    pin_size_fraction: float,
) -> tuple[float, float]:
    stub_ratio = _stub_margin_ratio(boundary_fraction, pin_size_fraction)
    rel_width = min(rel_width, _max_width_with_stubs(boundary_fraction, stub_ratio))
    min_left = boundary_fraction + stub_ratio * rel_width
    max_left = 1 - boundary_fraction - stub_ratio * rel_width - rel_width
    if min_left > max_left:
        return (min_left + max_left) / 2, rel_width
    return max(min_left, min(rel_left, max_left)), rel_width


def _balanced_grid_layout(
    children: list[Any],
    parent_aspect: float,
    title_fraction: float,
    child_aspects: dict[str, float],
    boundary_fraction: float,
    pin_size_fraction: float,
) -> dict[str, dict[str, Any]]:
    count = len(children)
    if count == 0:
        return {}
    cols = int((count ** 0.5) + 0.999999)
    rows = int((count / cols) + 0.999999) if cols > 0 else 0
    padding = GRID_LAYOUT_PADDING
    cell_w = (1.0 - padding * (cols + 1)) / cols if cols > 0 else 0.5
    cell_h = (1.0 - padding * (rows + 1)) / rows if rows > 0 else 0.5
    usable_y_fraction = max(0.1, 1.0 - title_fraction)
    stub_ratio = _stub_margin_ratio(boundary_fraction, pin_size_fraction)
    max_rel_width = _max_column_width_with_stubs(cols, boundary_fraction, stub_ratio)

    result: dict[str, dict[str, Any]] = {}
    for index, child in enumerate(sorted(children, key=lambda item: _natural_key(item.get_name()))):
        col = index % cols
        row = index // cols
        width_from_height = cell_h * usable_y_fraction * parent_aspect / max(child_aspects[child.get_id()], 0.01)
        rel_width = max(0.05, min(cell_w, width_from_height * GRID_CHILD_FILL_RATIO, max_rel_width))
        rel_x, rel_width = _constrain_child_horizontal(
            padding + col * (cell_w + padding) + max(0, cell_w - rel_width) / 2,
            rel_width,
            boundary_fraction,
            pin_size_fraction,
        )
        rel_pos = [
            rel_x,
            title_fraction + usable_y_fraction * (padding + row * (cell_h + padding)),
        ]
        result[child.get_name()] = {"rel_pos": rel_pos, "rel_width": rel_width}
    return result


def _layered_graph_layout(
    parent: Any,
    children: list[Any],
    parent_aspect: float,
    title_fraction: float,
    child_aspects: dict[str, float],
    boundary_fraction: float,
    pin_size_fraction: float,
) -> dict[str, dict[str, Any]]:
    if not children:
        return {}

    child_by_id = {child.get_id(): child for child in children}
    child_ids = list(child_by_id)
    input_driven: set[str] = set()
    output_drivers: set[str] = set()
    edges: list[tuple[str, str, str]] = []
    signal_edges_seen = False

    for wire in parent.get_wires():
        source_pin = wire.get_source_pin()
        source_owner_id = _component_owner_id(source_pin) if source_pin else None
        source_child_id = source_owner_id if source_owner_id in child_by_id else None

        for sink_pin in wire.get_sink_pins():
            sink_owner_id = _component_owner_id(sink_pin)
            sink_child_id = sink_owner_id if sink_owner_id in child_by_id else None
            sink_name = _pin_name(sink_pin).upper()
            if sink_name in CLOCK_PIN_NAMES:
                continue

            if sink_child_id:
                signal_edges_seen = True
                if source_child_id and source_child_id != sink_child_id:
                    edges.append((source_child_id, sink_child_id, sink_name))
                elif not source_child_id:
                    input_driven.add(sink_child_id)
            elif source_child_id:
                signal_edges_seen = True
                output_drivers.add(source_child_id)

    if not signal_edges_seen:
        return _balanced_grid_layout(
            children,
            parent_aspect,
            title_fraction,
            child_aspects,
            boundary_fraction,
            pin_size_fraction,
        )

    full_sccs = _tarjan_scc(child_ids, edges)
    cyclic_group_by_node: dict[str, int] = {}
    for index, group in enumerate(full_sccs):
        if len(group) > 1:
            for node in group:
                cyclic_group_by_node[node] = index

    active_edges: list[tuple[str, str, str]] = []
    for edge in edges:
        src, dst, sink_name = edge
        same_cycle = src in cyclic_group_by_node and cyclic_group_by_node.get(src) == cyclic_group_by_node.get(dst)
        dst_type = _component_type(child_by_id[dst])
        if same_cycle and dst_type in SEQUENTIAL_TYPES and sink_name in SEQUENTIAL_INPUT_PIN_NAMES:
            continue
        active_edges.append(edge)

    sccs = _tarjan_scc(child_ids, active_edges)
    scc_index_by_node: dict[str, int] = {}
    for index, group in enumerate(sccs):
        for node in group:
            scc_index_by_node[node] = index

    scc_edges: set[tuple[int, int]] = set()
    incoming_count: dict[int, int] = {index: 0 for index in range(len(sccs))}
    for src, dst, _sink_name in active_edges:
        src_scc = scc_index_by_node[src]
        dst_scc = scc_index_by_node[dst]
        if src_scc == dst_scc:
            continue
        if (src_scc, dst_scc) not in scc_edges:
            scc_edges.add((src_scc, dst_scc))
            incoming_count[dst_scc] += 1

    adjacency: dict[int, list[int]] = defaultdict(list)
    for src_scc, dst_scc in sorted(scc_edges):
        adjacency[src_scc].append(dst_scc)

    levels: dict[int, int] = {index: 0 for index in range(len(sccs))}
    for node in input_driven:
        levels[scc_index_by_node[node]] = max(levels[scc_index_by_node[node]], 1)

    queue: deque[int] = deque(sorted([index for index, count in incoming_count.items() if count == 0]))
    processed: list[int] = []
    while queue:
        current = queue.popleft()
        processed.append(current)
        for neighbor in adjacency[current]:
            levels[neighbor] = max(levels[neighbor], levels[current] + 1)
            incoming_count[neighbor] -= 1
            if incoming_count[neighbor] == 0:
                queue.append(neighbor)

    if len(processed) != len(sccs):
        return _balanced_grid_layout(
            children,
            parent_aspect,
            title_fraction,
            child_aspects,
            boundary_fraction,
            pin_size_fraction,
        )

    node_levels = {node: levels[scc_index_by_node[node]] for node in child_ids}
    levels_to_nodes: dict[int, list[str]] = defaultdict(list)
    for node in child_ids:
        levels_to_nodes[node_levels[node]].append(node)

    def sorted_naturally(nodes: list[str]) -> list[str]:
        return sorted(nodes, key=lambda node: _natural_key(child_by_id[node].get_name()))

    for level in list(levels_to_nodes):
        levels_to_nodes[level] = sorted_naturally(levels_to_nodes[level])

    predecessors: dict[str, list[str]] = defaultdict(list)
    successors: dict[str, list[str]] = defaultdict(list)
    for src, dst, _sink_name in active_edges:
        if node_levels.get(src) == node_levels.get(dst):
            continue
        successors[src].append(dst)
        predecessors[dst].append(src)

    level_values = sorted(levels_to_nodes)
    for _pass in range(4):
        order_index = {
            node: index
            for level in level_values
            for index, node in enumerate(levels_to_nodes[level])
        }
        for level in level_values[1:]:
            levels_to_nodes[level].sort(
                key=lambda node: (
                    sum(order_index[pred] for pred in predecessors[node]) / len(predecessors[node])
                    if predecessors[node]
                    else len(order_index),
                    _natural_key(child_by_id[node].get_name()),
                )
            )

        order_index = {
            node: index
            for level in level_values
            for index, node in enumerate(levels_to_nodes[level])
        }
        for level in reversed(level_values[:-1]):
            levels_to_nodes[level].sort(
                key=lambda node: (
                    sum(order_index[succ] for succ in successors[node]) / len(successors[node])
                    if successors[node]
                    else len(order_index),
                    _natural_key(child_by_id[node].get_name()),
                )
            )

    visual_columns: list[list[str]] = []
    max_rows_per_column = LAYERED_MAX_ROWS_PER_COLUMN
    for level in level_values:
        nodes = levels_to_nodes[level]
        chunk_count = max(1, (len(nodes) + max_rows_per_column - 1) // max_rows_per_column)
        chunk_size = max(1, (len(nodes) + chunk_count - 1) // chunk_count)
        for start in range(0, len(nodes), chunk_size):
            visual_columns.append(nodes[start : start + chunk_size])

    content_top = title_fraction
    content_height = max(0.1, 1.0 - title_fraction)
    padding_x = LAYERED_PADDING_X
    padding_y = LAYERED_PADDING_Y
    inner_top = content_top + content_height * padding_y
    inner_height = content_height * (1 - 2 * padding_y)
    visual_column_count = len(visual_columns)
    max_rows = max((len(nodes) for nodes in visual_columns), default=1)

    if len(children) == 1:
        rel_width = 0.8
    else:
        horizontal_cell = (1 - 2 * padding_x) / max(1, visual_column_count)
        rel_width = horizontal_cell * LAYERED_HORIZONTAL_FILL_RATIO

    for nodes in visual_columns:
        max_aspect = max(child_aspects[node] for node in nodes)
        rel_width = min(
            rel_width,
            (inner_height / max(1, len(nodes))) * parent_aspect / max(max_aspect, 0.01) * LAYERED_VERTICAL_FILL_RATIO,
        )

    rel_width = max(0.045, min(0.8, rel_width))
    if max_rows >= 10:
        rel_width = min(rel_width, LAYERED_DENSE_ROW_WIDTH_CAP)
    stub_ratio = _stub_margin_ratio(boundary_fraction, pin_size_fraction)
    rel_width = min(
        rel_width,
        _max_width_with_stubs(boundary_fraction, stub_ratio),
        _max_column_width_with_stubs(visual_column_count, boundary_fraction, stub_ratio),
    )

    left_center = boundary_fraction + stub_ratio * rel_width + rel_width / 2
    right_center = 1 - boundary_fraction - stub_ratio * rel_width - rel_width / 2
    denominator = max(1, visual_column_count - 1)
    result: dict[str, dict[str, Any]] = {}
    for column_index, nodes in enumerate(visual_columns):
        if visual_column_count == 1:
            x_center = 0.5
        else:
            x_center = left_center + (right_center - left_center) * (column_index / denominator)
        for row, node in enumerate(nodes):
            child_aspect = child_aspects[node]
            child_height_fraction = rel_width * child_aspect / max(parent_aspect, 0.01)
            y_center = inner_top + inner_height * ((row + 0.5) / max(1, len(nodes)))
            rel_x, fitted_width = _constrain_child_horizontal(
                x_center - rel_width / 2,
                rel_width,
                boundary_fraction,
                pin_size_fraction,
            )
            rel_pos = [
                rel_x,
                max(content_top, min(0.98 - child_height_fraction, y_center - child_height_fraction / 2)),
            ]
            result[child_by_id[node].get_name()] = {"rel_pos": rel_pos, "rel_width": fitted_width}

    return result


class LayoutManager:
    def __init__(self, filepath: Path):
        self.filepath = filepath
        try:
            data = json.loads(filepath.read_text())
        except FileNotFoundError:
            data = {}

        self.settings = data.get("default_settings", _clone(DEFAULT_SETTINGS))
        self.type_layouts = data.get("type_layouts", {})
        self.root_layouts = data.get("root_layouts", {})
        self.loaded_mtime = self._current_mtime()
        self.is_dirty = False

    def _current_mtime(self) -> int | None:
        try:
            return self.filepath.stat().st_mtime_ns
        except FileNotFoundError:
            return None

    def get_layout_for(self, component_type: str) -> dict[str, Any]:
        return _clone(self.type_layouts.get(component_type, {}))

    def get_root_layout_for(self, scenario_key: str, root_type: str) -> dict[str, Any]:
        type_layout = self.type_layouts.get(root_type, {})
        root_layout = self.root_layouts.get(scenario_key, {})
        merged = _clone(type_layout)
        merged.update(_clone(root_layout))
        merged["children"] = {
            **_clone(type_layout.get("children", {})),
            **_clone(root_layout.get("children", {})),
        }
        return merged

    def get_child_layout(
        self, parent_type: str, child_name: str, *, scenario_key: str | None = None, is_root: bool = False
    ) -> dict[str, Any]:
        parent_layout = (
            self.get_root_layout_for(scenario_key, parent_type)
            if is_root and scenario_key
            else self.get_layout_for(parent_type)
        )
        return _clone(parent_layout.get("children", {}).get(child_name, {}))

    def minimum_aspect_for_component(self, component: Any) -> float:
        return _minimum_aspect_ratio_for_pins(component, self.settings)

    @staticmethod
    def _coerced_aspect(layout: dict[str, Any], fallback: float, minimum: float) -> float:
        if "aspect_ratio" not in layout:
            return round(minimum, 3)
        return round(max(_float_value(layout.get("aspect_ratio"), fallback), minimum), 3)

    def effective_minimum_aspect_for_component(
        self, component: Any, *, scenario_key: str | None = None, is_root: bool = False
    ) -> float:
        instance_minimum = self.minimum_aspect_for_component(component)
        layout = (
            self.get_root_layout_for(scenario_key, component.get_type_name())
            if is_root and scenario_key
            else self.get_layout_for(component.get_type_name())
        )
        return round(max(instance_minimum, _float_value(layout.get("min_aspect_ratio"), instance_minimum)), 3)

    def aspect_for_component(self, component: Any, *, scenario_key: str | None = None, is_root: bool = False) -> float:
        fallback = 0.75 if is_root else 1.0
        minimum = self.effective_minimum_aspect_for_component(
            component,
            scenario_key=scenario_key,
            is_root=is_root,
        )
        layout = (
            self.get_root_layout_for(scenario_key, component.get_type_name())
            if is_root and scenario_key
            else self.get_layout_for(component.get_type_name())
        )
        return self._coerced_aspect(layout, fallback, minimum)

    def ensure_component_layout_defaults(
        self, component: Any, *, scenario_key: str | None = None, is_root: bool = False
    ) -> None:
        component_type = component.get_type_name()
        if scenario_key and self.should_use_root_layout_for_defaults(scenario_key, component_type, is_root):
            layout = self.root_layouts.setdefault(scenario_key, {})
            fallback = 0.75
        else:
            layout = self.type_layouts.setdefault(component_type, {})
            fallback = 1.0

        instance_minimum = self.minimum_aspect_for_component(component)
        minimum = round(max(instance_minimum, _float_value(layout.get("min_aspect_ratio"), instance_minimum)), 3)
        aspect = self._coerced_aspect(layout, fallback, minimum)
        if layout.get("min_aspect_ratio") != minimum:
            layout["min_aspect_ratio"] = minimum
            self.is_dirty = True
        if layout.get("aspect_ratio") != aspect:
            layout["aspect_ratio"] = aspect
            self.is_dirty = True

    def update_root_child_layout(
        self, scenario_key: str, child_name: str, rel_pos: list[float], rel_width: float
    ) -> None:
        root_layout = self.root_layouts.setdefault(scenario_key, {})
        child_entry = root_layout.setdefault("children", {}).setdefault(child_name, {})
        if child_entry.get("rel_pos") == rel_pos and child_entry.get("rel_width") == rel_width:
            return
        child_entry["rel_pos"] = rel_pos
        child_entry["rel_width"] = rel_width
        self.is_dirty = True

    def update_type_child_layout(
        self, parent_type: str, child_name: str, rel_pos: list[float], rel_width: float
    ) -> None:
        type_layout = self.type_layouts.setdefault(parent_type, {})
        child_entry = type_layout.setdefault("children", {}).setdefault(child_name, {})
        if child_entry.get("rel_pos") == rel_pos and child_entry.get("rel_width") == rel_width:
            return
        child_entry["rel_pos"] = rel_pos
        child_entry["rel_width"] = rel_width
        self.is_dirty = True

    def should_use_root_layout_for_defaults(self, scenario_key: str, parent_type: str, is_root: bool) -> bool:
        return is_root and (parent_type in GENERIC_ROOT_TYPES or scenario_key in self.root_layouts)

    def set_child_layouts(
        self,
        parent_type: str,
        placements: dict[str, dict[str, Any]],
        *,
        scenario_key: str | None = None,
        is_root: bool = False,
    ) -> None:
        for child_name, placement in placements.items():
            rel_pos = placement["rel_pos"]
            rel_width = placement["rel_width"]
            if scenario_key and self.should_use_root_layout_for_defaults(scenario_key, parent_type, is_root):
                self.update_root_child_layout(scenario_key, child_name, rel_pos, rel_width)
            else:
                self.update_type_child_layout(parent_type, child_name, rel_pos, rel_width)

    def to_jsonable(self) -> dict[str, Any]:
        return {
            "default_settings": self.settings,
            "type_layouts": self.type_layouts,
            "root_layouts": self.root_layouts,
        }

    def replace_from_client(self, layout: dict[str, Any]) -> None:
        self.settings = _clone(layout.get("default_settings", {}))
        self.type_layouts = _clone(layout.get("type_layouts", {}))
        self.root_layouts = _clone(layout.get("root_layouts", {}))
        self.is_dirty = True

    def save(self) -> None:
        self.filepath.parent.mkdir(parents=True, exist_ok=True)
        pretty = json.dumps(self.to_jsonable(), indent=4)

        def format_list_content(match: re.Match[str]) -> str:
            compact = re.sub(r"\s+", "", match.group(1)).replace(",", ", ")
            return f"[{compact}]"

        final = re.sub(r"\[(.*?)\]", format_list_content, pretty, flags=re.DOTALL)
        self.filepath.write_text(final)
        self.loaded_mtime = self._current_mtime()
        self.is_dirty = False

    def save_if_dirty(self) -> None:
        if self.is_dirty:
            self.save()


def scenario_default_alias(class_name: str) -> str:
    stem = class_name[:-4] if class_name.endswith("Test") else class_name
    stem = stem.replace("_", "-")
    stem = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1-\2", stem)
    stem = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", stem)
    return stem.lower()


def scenario_canonical_alias(class_name: str) -> str:
    return PREFERRED_SCENARIO_ALIASES.get(class_name, [scenario_default_alias(class_name)])[0]


def visualizable_test_names() -> list[str]:
    return [
        class_name
        for class_name in circuit_backend.get_registered_test_names()
        if class_name not in NON_VISUALIZABLE_TESTS and hasattr(circuit_backend, class_name)
    ]


def _scenario_classes() -> dict[str, type]:
    # Expose registry tests that build a reusable topology during setup_circuit().
    # The excluded tests intentionally verify API/validation behavior without a
    # meaningful visual root.

    scenarios: dict[str, type] = {}
    for class_name in visualizable_test_names():
        scenario_class = getattr(circuit_backend, class_name)
        aliases = {scenario_default_alias(class_name), class_name.lower(), *PREFERRED_SCENARIO_ALIASES.get(class_name, [])}
        for alias in aliases:
            scenarios[alias] = scenario_class
    return scenarios


def _scenario_options() -> list[dict[str, str]]:
    return [
        {"key": scenario_canonical_alias(class_name), "className": class_name}
        for class_name in visualizable_test_names()
    ]


def _scenario_aliases_to_canonical() -> dict[str, str]:
    aliases_to_canonical: dict[str, str] = {}
    for class_name in visualizable_test_names():
        canonical = scenario_canonical_alias(class_name)
        aliases = {scenario_default_alias(class_name), class_name.lower(), *PREFERRED_SCENARIO_ALIASES.get(class_name, [])}
        for alias in aliases:
            aliases_to_canonical[alias] = canonical
    return aliases_to_canonical


SCENARIOS = _scenario_classes()
SCENARIO_OPTIONS = _scenario_options()
SCENARIO_ALIASES_TO_CANONICAL = _scenario_aliases_to_canonical()
DEFAULT_SCENARIO = "adder8" if "adder8" in SCENARIOS else SCENARIO_OPTIONS[0]["key"]


class CircuitSession:
    def __init__(self, scenario_key: str, layout_manager: LayoutManager):
        self.scenario_key = normalize_scenario(scenario_key)
        self.layout_manager = layout_manager
        self.lock = threading.Lock()

        self.test_scenario = SCENARIOS[self.scenario_key]()
        self.test_scenario.setup_circuit()
        self.root = self.test_scenario.get_root()
        if not self.root:
            raise RuntimeError("C++ test scenario did not produce a root component")

        self.simulator = self.test_scenario.get_simulator()
        self.simulator.run_and_record(self.test_scenario.get_run_duration())
        checkpoint_metadata = self._checkpoint_metadata()
        checkpoint_times = [checkpoint["time"] for checkpoint in checkpoint_metadata]
        self.timestamps = sorted({int(time) for time in self.simulator.get_unique_timestamps()} | set(checkpoint_times))
        if not self.timestamps:
            self.timestamps = [0]
        timestamp_index = {timestamp: index for index, timestamp in enumerate(self.timestamps)}
        self.checkpoints = [
            {
                **checkpoint,
                "index": timestamp_index[checkpoint["time"]],
            }
            for checkpoint in checkpoint_metadata
            if checkpoint["time"] in timestamp_index
        ]
        self.simulator.set_circuit_state_at_time(self.timestamps[0])

        self.components: list[dict[str, Any]] = []
        self.pins: list[dict[str, Any]] = []
        self.wires: list[dict[str, Any]] = []
        self.component_handles: dict[str, Any] = {}
        self.component_depths: dict[str, int] = {}
        self.pin_handles: dict[str, Any] = {}
        self.wire_handles: dict[str, Any] = {}
        self._build_topology()
        self.layout_manager.save_if_dirty()
        if len(self.components) == 1 and not self.pins and not self.wires:
            raise RuntimeError(
                f"Scenario '{scenario_key}' did not expose a visual circuit topology. "
                "Its C++ test likely creates local components only during verifyResults()."
            )

    def _checkpoint_metadata(self) -> list[dict[str, Any]]:
        try:
            checkpoints = list(self.test_scenario.get_checkpoints())
        except AttributeError:
            return []

        result: list[dict[str, Any]] = []
        for index, checkpoint in enumerate(checkpoints):
            time = int(getattr(checkpoint, "time", 0))
            label = str(getattr(checkpoint, "label", "") or f"Checkpoint {index}")
            detail = str(getattr(checkpoint, "detail", ""))
            row_index = int(getattr(checkpoint, "row_index", index))
            result.append(
                {
                    "time": time,
                    "label": label,
                    "detail": detail,
                    "rowIndex": row_index,
                }
            )
        return sorted(result, key=lambda item: (item["time"], item["rowIndex"], item["label"]))

    def _component_layout_meta(self, component: Any) -> tuple[float, list[int]]:
        is_root = component.get_id() == self.root.get_id()
        aspect_ratio = self.layout_manager.aspect_for_component(
            component,
            scenario_key=self.scenario_key,
            is_root=is_root,
        )
        if is_root:
            layout = self.layout_manager.get_root_layout_for(self.scenario_key, component.get_type_name())
        else:
            layout = self.layout_manager.get_layout_for(component.get_type_name())
        return aspect_ratio, layout.get("color", DEFAULT_COLOR)

    def _build_topology(self) -> None:
        def walk(component: Any, parent_id: str | None, depth: int) -> None:
            component_id = component.get_id()
            component_type = component.get_type_name()
            children = list(component.get_children())
            self.layout_manager.ensure_component_layout_defaults(
                component,
                scenario_key=self.scenario_key,
                is_root=depth == 0,
            )
            aspect_ratio, color = self._component_layout_meta(component)
            self.component_handles[component_id] = component
            self.component_depths[component_id] = depth

            self.components.append(
                {
                    "id": component_id,
                    "name": component.get_name(),
                    "type": component_type,
                    "parentId": parent_id,
                    "depth": depth,
                    "childIds": [child.get_id() for child in children],
                    "aspectRatio": aspect_ratio,
                    "minAspectRatio": self.layout_manager.effective_minimum_aspect_for_component(
                        component,
                        scenario_key=self.scenario_key,
                        is_root=depth == 0,
                    ),
                    "color": color,
                }
            )

            if hasattr(component, "get_input_pins"):
                for pin_name, pin in component.get_input_pins().items():
                    self._add_pin(component_id, pin_name, pin, "input")
                for pin_name, pin in component.get_output_pins().items():
                    self._add_pin(component_id, pin_name, pin, "output")

            self._add_wires(component)
            self._ensure_child_defaults(component, children, depth)
            for child in children:
                walk(child, component_id, depth + 1)

        walk(self.root, None, 0)

    def _add_pin(self, component_id: str, pin_name: str, pin: Any, pin_type: str) -> None:
        try:
            pin_id = pin.get_id()
        except AttributeError:
            pin_id = f"{component_id}.{pin_name}"
        self.pin_handles[pin_id] = pin
        self.pins.append(
            {
                "id": pin_id,
                "componentId": component_id,
                "name": pin_name,
                "type": pin_type,
                "width": _signal_width(pin),
            }
        )

    def _add_wires(self, component: Any) -> None:
        for wire in component.get_wires():
            wire_id = wire.get_id()
            self.wire_handles[wire_id] = wire
            source_pin = wire.get_source_pin()
            source_pin_id = source_pin.get_id() if source_pin else None
            sink_pin_ids = [pin.get_id() for pin in wire.get_sink_pins()]
            self.wires.append(
                {
                    "id": wire_id,
                    "name": wire.get_name(),
                    "ownerId": component.get_id(),
                    "sourcePinId": source_pin_id,
                    "sinkPinIds": sink_pin_ids,
                    "width": _signal_width(wire),
                }
            )

    def _default_child_layouts(
        self,
        component: Any,
        children: list[Any],
        depth: int,
        layout_manager: LayoutManager | None = None,
    ) -> dict[str, dict[str, Any]]:
        manager = layout_manager or self.layout_manager
        is_root = depth == 0
        parent_aspect = manager.aspect_for_component(component, scenario_key=self.scenario_key, is_root=is_root)
        title_ratio = float(manager.settings.get("title_bar_ratio", 0.15))
        title_fraction = min(0.7, title_ratio / max(parent_aspect, 0.01))
        boundary_fraction = float(manager.settings.get("boundary_area_ratio", DEFAULT_SETTINGS["boundary_area_ratio"]))
        pin_size_fraction = float(manager.settings.get("pin_size_ratio", DEFAULT_SETTINGS["pin_size_ratio"]))
        child_aspects: dict[str, float] = {}
        for child in children:
            child_aspects[child.get_id()] = manager.aspect_for_component(child)

        return _layered_graph_layout(
            component,
            children,
            parent_aspect,
            title_fraction,
            child_aspects,
            boundary_fraction,
            pin_size_fraction,
        )

    def _ensure_child_defaults(self, component: Any, children: list[Any], depth: int) -> None:
        count = len(children)
        if count == 0:
            return

        parent_type = component.get_type_name()
        is_root = depth == 0
        missing_children = [
            child
            for child in children
            if not (
                "rel_pos" in self.layout_manager.get_child_layout(
                    parent_type,
                    child.get_name(),
                    scenario_key=self.scenario_key,
                    is_root=is_root,
                )
                and "rel_width" in self.layout_manager.get_child_layout(
                    parent_type,
                    child.get_name(),
                    scenario_key=self.scenario_key,
                    is_root=is_root,
                )
            )
        ]
        if not missing_children:
            return

        placements = self._default_child_layouts(component, children, depth)
        missing_placements = {
            child.get_name(): placements[child.get_name()]
            for child in missing_children
            if child.get_name() in placements
        }
        self.layout_manager.set_child_layouts(
            parent_type,
            missing_placements,
            scenario_key=self.scenario_key,
            is_root=is_root,
        )

        for child in children:
            child_layout = self.layout_manager.get_child_layout(
                parent_type,
                child.get_name(),
                scenario_key=self.scenario_key,
                is_root=is_root,
            )
            if "rel_pos" not in child_layout or "rel_width" not in child_layout:
                raise RuntimeError(f"Layout generation failed for {parent_type}.{child.get_name()}")

    def default_child_layouts(
        self,
        parent_id: str,
        child_name: str | None = None,
        layout: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        parent = self.component_handles.get(parent_id)
        if parent is None:
            raise ValueError(f"Unknown parent component id '{parent_id}'")
        children = list(parent.get_children())
        depth = self.component_depths[parent_id]
        manager = self.layout_manager
        if layout is not None:
            manager = LayoutManager(LAYOUT_PATH)
            manager.replace_from_client(layout)
            manager.is_dirty = False
        placements = self._default_child_layouts(parent, children, depth, manager)
        if child_name:
            if child_name not in placements:
                raise ValueError(f"Parent '{parent.get_name()}' has no child layout named '{child_name}'")
            placements = {child_name: placements[child_name]}
        return {
            "scenario": self.scenario_key,
            "parentId": parent_id,
            "parentType": parent.get_type_name(),
            "isRoot": depth == 0,
            "placements": placements,
        }

    def topology_response(self) -> dict[str, Any]:
        return {
            "scenario": self.scenario_key,
            "rootId": self.root.get_id(),
            "rootType": self.root.get_type_name(),
            "components": self.components,
            "pins": self.pins,
            "wires": self.wires,
            "timestamps": self.timestamps,
            "checkpoints": self.checkpoints,
            "layout": self.layout_manager.to_jsonable(),
            "state": self.state_at_index(0),
            "stats": {
                "componentCount": len(self.components),
                "pinCount": len(self.pins),
                "wireCount": len(self.wires),
                "timestampCount": len(self.timestamps),
            },
        }

    def state_at_index(self, index: int) -> dict[str, Any]:
        with self.lock:
            index = max(0, min(index, len(self.timestamps) - 1))
            timestamp = int(self.timestamps[index])
            self.simulator.set_circuit_state_at_time(timestamp)
            pins = {pin_id: _signal_value(pin) for pin_id, pin in self.pin_handles.items()}
            wires = {wire_id: _signal_value(wire) for wire_id, wire in self.wire_handles.items()}
            return {"index": index, "time": timestamp, "pins": pins, "wires": wires}


class SessionStore:
    def __init__(self) -> None:
        self.layout_manager = LayoutManager(LAYOUT_PATH)
        self.sessions: dict[str, CircuitSession] = {}
        self.lock = threading.Lock()

    def _reload_layout_if_changed(self) -> None:
        current_mtime = self.layout_manager._current_mtime()
        if current_mtime != self.layout_manager.loaded_mtime:
            self.layout_manager = LayoutManager(LAYOUT_PATH)
            self.sessions.clear()

    def get(self, scenario: str) -> CircuitSession:
        scenario_key = normalize_scenario(scenario)
        with self.lock:
            self._reload_layout_if_changed()
            if scenario_key not in self.sessions:
                self.sessions[scenario_key] = CircuitSession(scenario_key, self.layout_manager)
            return self.sessions[scenario_key]

    def replace_layout(self, layout: dict[str, Any]) -> None:
        with self.lock:
            self.layout_manager.replace_from_client(layout)
            self.layout_manager.save()
            self.sessions.clear()

    def default_child_layouts(
        self,
        scenario: str,
        parent_id: str,
        child_name: str | None = None,
        layout: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        scenario_key = normalize_scenario(scenario)
        with self.lock:
            self._reload_layout_if_changed()
            if scenario_key not in self.sessions:
                self.sessions[scenario_key] = CircuitSession(scenario_key, self.layout_manager)
            return self.sessions[scenario_key].default_child_layouts(parent_id, child_name, layout)


STORE = SessionStore()


def normalize_scenario(scenario: str | None) -> str:
    key = (scenario or DEFAULT_SCENARIO).strip().lower().replace("_", "-")
    if key not in SCENARIOS:
        valid = ", ".join(sorted(SCENARIOS))
        raise ValueError(f"Unknown scenario '{scenario}'. Valid scenarios: {valid}")
    return SCENARIO_ALIASES_TO_CANONICAL.get(key, key)


def json_response(handler: SimpleHTTPRequestHandler, payload: Any, status: int = 200) -> None:
    encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(encoded)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(encoded)


def error_response(handler: SimpleHTTPRequestHandler, message: str, status: int = 400) -> None:
    json_response(handler, {"error": message}, status)


class VisualizerHandler(SimpleHTTPRequestHandler):
    server_version = "CircuitSimVisualizerV2/1.0"

    def _normalize_path(self, path: str) -> str:
        api_index = path.find("/api/")
        if api_index >= 0:
            return path[api_index:]
        if path.endswith("/api"):
            return "/api"
        absproxy_index = path.find("/absproxy/")
        if absproxy_index >= 0:
            parts = path[absproxy_index:].split("/", 3)
            if len(parts) >= 4:
                return "/" + parts[3]
            return "/"
        return path

    def _base_href_for_request(self, request_path: str) -> str:
        absproxy_index = request_path.find("/absproxy/")
        if absproxy_index >= 0:
            parts = request_path[absproxy_index:].split("/", 3)
            if len(parts) >= 3 and parts[2]:
                return f"/absproxy/{parts[2]}/"
        return "/"

    def do_GET(self) -> None:  # noqa: N802 - http.server API
        parsed = urlparse(self.path)
        raw_path = parsed.path
        path = self._normalize_path(raw_path)
        query = parse_qs(parsed.query)

        try:
            if path == "/api/health":
                json_response(self, {"ok": True})
            elif path == "/api/scenarios":
                json_response(self, {"default": DEFAULT_SCENARIO, "scenarios": SCENARIO_OPTIONS})
            elif path == "/api/circuit":
                scenario = query.get("scenario", [DEFAULT_SCENARIO])[0]
                json_response(self, STORE.get(scenario).topology_response())
            elif path == "/api/state":
                scenario = query.get("scenario", [DEFAULT_SCENARIO])[0]
                index = int(query.get("index", [0])[0])
                json_response(self, STORE.get(scenario).state_at_index(index))
            else:
                self._serve_static(path, raw_path)
        except Exception as exc:
            error_response(self, str(exc), 500 if not isinstance(exc, ValueError) else 400)

    def do_POST(self) -> None:  # noqa: N802 - http.server API
        parsed = urlparse(self.path)
        path = self._normalize_path(parsed.path)
        if path not in {"/api/layout", "/api/layout/defaults"}:
            error_response(self, "Unknown POST endpoint", 404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length).decode("utf-8")
            payload = json.loads(body or "{}")
            if path == "/api/layout":
                layout = payload.get("layout")
                if not isinstance(layout, dict):
                    raise ValueError("POST /api/layout requires a layout object")
                STORE.replace_layout(layout)
                json_response(self, {"ok": True, "path": str(LAYOUT_PATH)})
                return

            scenario = payload.get("scenario")
            parent_id = payload.get("parentId")
            child_name = payload.get("childName")
            layout = payload.get("layout")
            if not isinstance(parent_id, str):
                raise ValueError("POST /api/layout/defaults requires parentId")
            if child_name is not None and not isinstance(child_name, str):
                raise ValueError("childName must be a string when provided")
            if layout is not None and not isinstance(layout, dict):
                raise ValueError("layout must be an object when provided")
            json_response(
                self,
                STORE.default_child_layouts(
                    scenario if isinstance(scenario, str) else DEFAULT_SCENARIO,
                    parent_id,
                    child_name,
                    layout,
                ),
            )
        except Exception as exc:
            error_response(self, str(exc), 400)

    def _serve_static(self, request_path: str, raw_path: str | None = None) -> None:
        safe_path = unquote(request_path).split("?", 1)[0]
        if safe_path in ("", "/"):
            safe_path = "/index.html"
        target = (STATIC_DIR / safe_path.lstrip("/")).resolve()
        if not str(target).startswith(str(STATIC_DIR.resolve())) or not target.exists() or target.is_dir():
            if "." not in Path(safe_path).name:
                target = STATIC_DIR / "index.html"
            elif Path(safe_path).name in {"styles.css", "app.js"}:
                target = STATIC_DIR / Path(safe_path).name
            else:
                self.send_error(404, "File not found")
                return
        if not str(target.resolve()).startswith(str(STATIC_DIR.resolve())) or not target.exists() or target.is_dir():
            self.send_error(404, "File not found")
            return

        if target.name == "index.html":
            html = target.read_text()
            base_href = self._base_href_for_request(raw_path or request_path)
            html = html.replace("<head>", f'<head>\n  <base href="{base_href}">', 1)
            content = html.encode("utf-8")
        else:
            content = target.read_bytes()
        mime_type = mimetypes.guess_type(str(target))[0] or "application/octet-stream"
        self.send_response(200)
        self.send_header("Content-Type", mime_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def find_available_port(host: str, preferred_port: int) -> int:
    for port in range(preferred_port, preferred_port + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.bind((host, port))
            except OSError:
                continue
            return port
    raise RuntimeError(f"No free port found from {preferred_port} to {preferred_port + 99}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the CircuitSim HTML visualizer.")
    parser.add_argument("--host", default=os.environ.get("VISUALIZER_V2_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("VISUALIZER_V2_PORT", "8765")))
    args = parser.parse_args()

    port = find_available_port(args.host, args.port)
    server = ThreadingHTTPServer((args.host, port), VisualizerHandler)
    print(f"CircuitSim visualizer v2 running at http://{args.host}:{port}/")
    print(f"Layout file: {LAYOUT_PATH}")
    server.serve_forever()


if __name__ == "__main__":
    main()
