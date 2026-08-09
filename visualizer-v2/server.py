#!/usr/bin/env python3
"""Browser-based CircuitSim visualizer.

This server intentionally uses only the Python standard library plus the
existing pybind11 `circuit_backend` module. The browser owns rendering and
interaction; Python owns circuit discovery, simulator state scrubbing, and
layout persistence.
"""

from __future__ import annotations

import argparse
from collections import OrderedDict, defaultdict, deque
import copy
import gzip
import json
import mimetypes
import os
from pathlib import Path
import re
import socket
import sys
import threading
import uuid
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse

from profile_store import ProfileStore, validate_overrides


ROOT_DIR = Path(__file__).resolve().parents[1]
APP_DIR = Path(__file__).resolve().parent
STATIC_DIR = APP_DIR / "static"
LAYOUT_PATH = APP_DIR / "layout.json"
PROFILE_PATH = APP_DIR / "profiles.json"
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
LAYOUT_SCHEMA_VERSION = 2
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
MIN_RELATIVE_CHILD_WIDTH = 1e-6
COMPONENT_LAYOUT_DEFAULTS = {
    # Program scenarios use a fixed, shallow testbench container around the
    # selectable CPU and memory implementations.  Keep the outside wide like
    # a system block diagram for both CPU families.
    "RV32IProgramRoot": {
        "aspect_ratio": 0.65,
        "title_bar_ratio": 0.055,
        "pin_size_ratio": 0.025,
    },
    # The five-stage core has 35 logical output pins. Its pipeline is
    # horizontal, so use smaller edge markers and a shorter title bar instead
    # of making the whole component four times taller than it is wide.
    "RV32IFiveStageCore": {
        "aspect_ratio": 0.6,
        "title_bar_ratio": 0.05,
        "pin_size_ratio": 0.012,
    },
    # The coordinator is a horizontal control rail beneath the five data
    # stages, not a tall standalone box in this context.
    "RV32IPipelineCoordinator": {
        "aspect_ratio": 0.35,
        "title_bar_ratio": 0.04,
        "pin_size_ratio": 0.01,
    },
}
SEQUENTIAL_TYPES = {"DFlipFlop"}
CLOCK_PIN_NAMES = {"CLK", "CLOCK"}
SEQUENTIAL_INPUT_PIN_NAMES = {"D", "RST", "RESET", "SET", "CLR", "CLEAR", "EN", "ENABLE", "LOAD"}
NON_VISUALIZABLE_TESTS = {
    "WireTemplateTest",
    "RewireValidationTest",
}
PREFERRED_SCENARIO_ALIASES = {
    "FullAdderTest": ["full-adder", "fulladder"],
    "HalfAdderTest": ["half-adder", "halfadder"],
    "FullCircuitTest": ["full-circuit", "fullcircuit"],
    "SRLatchTest": ["sr-latch", "srlatch"],
    "GatedDLatchTest": ["gated-d-latch", "gateddlatch", "d-latch"],
    "DFlipFlopTest": ["dff", "d-flip-flop"],
    "ClockGeneratorTest": ["clock"],
    "MemoryBitTest": ["memory-bit", "memorybit"],
    "Memory64Kx32Test": ["memory64kx32", "memory-64kx32"],
    "Register32Test": ["register32", "register-32"],
    "RegisterFile4x32Test": ["register-file4x32", "register-file-4x32", "registerfile4x32", "rf4x32"],
    "RegisterFile32x32Test": ["rv32i-register-file", "register-file32x32", "register-file-32x32", "registerfile32x32", "rf32x32"],
    "Memory4x32Test": ["memory4x32", "memory-4x32", "mem4x32"],
    "Memory32x32Test": ["memory32x32", "memory-32x32", "mem32x32"],
    "ConstantValue1Test": ["constant1"],
    "ConstantValue2Test": ["constant2"],
    "ConstantValue4Test": ["constant4"],
    "ConstantValue32Test": ["constant32"],
    "Adder8Test": ["adder8", "8-bit-adder"],
    "ZeroDetect8Test": ["zero-detect8"],
    "ALU8Test": ["alu8"],
    "Mux32to1Test": ["mux32to1"],
    "Mux4to1_32bitTest": ["mux4to1-32bit"],
    "Mux32to1_32bitTest": ["mux32to1-32bit"],
    "Decoder2to4Test": ["decoder2to4", "decoder-2to4", "decoder-2-4"],
    "Decoder5to32Test": ["decoder5to32", "decoder-5to32", "decoder-5-32"],
    "RewireTest": ["rewire"],
    "Adder32Test": ["adder32"],
    "AddSub32Test": ["addsub32", "add-sub32"],
    "Logic32Test": ["logic32"],
    "ZeroDetect32Test": ["zero-detect32"],
    "Comparator32Test": ["comparator32"],
    "Shifter32Test": ["shifter32"],
    "ALU32Test": ["alu32", "rv32i-alu32"],
    "ALU32RepresentativeSliceTest": ["alu32-slice"],
    "RV32ISingleCycleCoreTest": ["rv32i-core"],
    "RV32IFiveStageCoreTest": ["rv32i-five-stage-core", "rv32i-pipeline-core"],
    "RV32IControlFlowUnitTest": ["rv32i-control-flow"],
    "RV32IDecodeControlUnitTest": ["rv32i-decode-control", "rv32i-decode"],
    "RV32IExecutionControlStatusUnitTest": ["rv32i-execution-status", "rv32i-status"],
    "RV32IProgramLoaderTest": ["rv32i-program-loader", "rv32i-program", "program-loader"],
}

_PROGRAM_TEST_NAME = re.compile(
    r"^(RV32ISingleCycleSystemTest|RV32IFiveStageCoreProgramTest)"
    r"/program-([0-9]+)$"
)
for _test_name in circuit_backend.get_registered_test_names():
    _program_match = _PROGRAM_TEST_NAME.fullmatch(_test_name)
    if not _program_match:
        continue
    _architecture, _number_text = _program_match.groups()
    _program_number = int(_number_text)
    _alias_prefix = (
        "rv32i-program"
        if _architecture == "RV32ISingleCycleSystemTest"
        else "rv32i-five-stage-program"
    )
    PREFERRED_SCENARIO_ALIASES[_test_name] = [
        f"{_alias_prefix}{_program_number}"
    ]


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


def _logic_vector_value(values: Any) -> str:
    bits = [_logic_token(value) for value in values]
    if not bits:
        return ""
    return bits[0] if len(bits) == 1 else "".join(reversed(bits))


def _binary_value_to_hex(value: str) -> str | None:
    if not value or any(bit not in {"0", "1"} for bit in value):
        return None
    width = max(1, (len(value) + 3) // 4)
    return f"0x{int(value, 2):0{width}x}"


def _binary_value_to_int(value: str | None) -> int | None:
    if not value or any(bit not in {"0", "1"} for bit in value):
        return None
    return int(value, 2)


def _memory_word_entry(address: int, word: Any, decode_instruction: bool = False) -> dict[str, Any]:
    bits = _logic_vector_value(word)
    hex_value = _binary_value_to_hex(bits)
    entry = {
        "address": int(address),
        "addressHex": f"0x{int(address):08x}",
        "bits": bits,
        "hex": hex_value,
        "known": hex_value is not None,
    }
    if decode_instruction and hex_value is not None:
        try:
            entry["instructionText"] = circuit_backend.disassemble_rv32i_instruction(int(bits, 2), int(address))
        except (AttributeError, RuntimeError, TypeError, ValueError):
            entry["instructionText"] = None
    return entry


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


def _component_profile_fingerprint(component: Any) -> str:
    try:
        fingerprint = str(component.get_profile_fingerprint())
    except (AttributeError, RuntimeError, TypeError, ValueError):
        return "explicit"
    return fingerprint or "explicit"


def _pin_width_by_name(component: Any, getter_name: str, pin_name: str) -> int | None:
    try:
        pins = getattr(component, getter_name)()
    except (AttributeError, RuntimeError):
        return None
    try:
        pin = pins.get(pin_name)
    except AttributeError:
        pin = None
    return _signal_width(pin) if pin else None


def _component_layout_type(component: Any) -> str:
    component_type = _component_type(component)
    if component_type == "BitSplitter":
        width = _pin_width_by_name(component, "get_input_pins", "IN")
        return f"BitSplitter<{width}>" if width else component_type
    if component_type == "BitJoiner":
        width = _pin_width_by_name(component, "get_output_pins", "OUT")
        return f"BitJoiner<{width}>" if width else component_type
    if component_type == "ConstantValue":
        out_width = _pin_width_by_name(component, "get_output_pins", "OUT")
        if out_width:
            return f"ConstantValue<{out_width}>"
    if component_type == "RV32IBitPatternMatcher":
        try:
            mask = int(component.mask)
            value = int(component.value)
        except (AttributeError, RuntimeError, TypeError, ValueError):
            return component_type
        return f"RV32IBitPatternMatcher<mask=0x{mask:08x},value=0x{value:08x}>"
    return component_type


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


def _minimum_aspect_ratio_for_pins(
    component: Any,
    settings: dict[str, Any],
    layout: dict[str, Any] | None = None,
) -> float:
    layout = layout or {}
    title_ratio = max(
        0.0,
        _float_value(
            layout.get("title_bar_ratio"),
            _float_value(
                settings.get("title_bar_ratio"),
                DEFAULT_SETTINGS["title_bar_ratio"],
            ),
        ),
    )
    pin_size_ratio = max(
        0.0,
        _float_value(
            layout.get("pin_size_ratio"),
            _float_value(
                settings.get("pin_size_ratio"),
                DEFAULT_SETTINGS["pin_size_ratio"],
            ),
        ),
    )
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
        rel_width = max(
            MIN_RELATIVE_CHILD_WIDTH,
            min(cell_w, width_from_height * GRID_CHILD_FILL_RATIO, max_rel_width),
        )
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


FIVE_STAGE_PIPELINE_CHILD_ORDER = (
    "FETCH",
    "IF_ID",
    "DECODE",
    "ID_EX",
    "EXECUTE",
    "EX_MEM",
    "MEMORY",
    "MEM_WB",
    "WRITEBACK",
)


def _five_stage_pipeline_layout(
    children: list[Any],
    parent_aspect: float,
    title_fraction: float,
    child_aspects: dict[str, float],
    boundary_fraction: float,
    pin_size_fraction: float,
) -> dict[str, dict[str, Any]] | None:
    """Place the CPU as a readable left-to-right pipeline plus control rail."""
    child_by_name = {
        child.get_name(): child
        for child in children
    }
    required_names = {
        *FIVE_STAGE_PIPELINE_CHILD_ORDER,
        "COORDINATOR",
    }
    if set(child_by_name) != required_names:
        return None

    content_top = title_fraction
    content_height = max(0.1, 1.0 - content_top)
    pipeline_top = content_top + content_height * 0.04
    pipeline_height = content_height * 0.60
    coordinator_top = content_top + content_height * 0.72
    coordinator_height = content_height * 0.22
    stub_ratio = _stub_margin_ratio(
        boundary_fraction, pin_size_fraction
    )
    outer_width_factor = 1 + 2 * stub_ratio
    available_width = max(
        0.1, 1 - 2 * boundary_fraction
    )

    weights = {
        name: (
            0.42
            if name in {"IF_ID", "ID_EX", "EX_MEM", "MEM_WB"}
            else 1.0
        )
        for name in FIVE_STAGE_PIPELINE_CHILD_ORDER
    }
    body_budget = available_width * 0.88 / outer_width_factor
    weight_total = sum(weights.values())
    widths: dict[str, float] = {}
    for name in FIVE_STAGE_PIPELINE_CHILD_ORDER:
        child = child_by_name[name]
        height_limited_width = (
            pipeline_height
            * 0.86
            * parent_aspect
            / max(child_aspects[child.get_id()], 0.01)
        )
        widths[name] = min(
            body_budget * weights[name] / weight_total,
            height_limited_width,
        )

    used_outer_width = sum(
        width * outer_width_factor
        for width in widths.values()
    )
    gap = max(
        0.0,
        (available_width - used_outer_width)
        / (len(FIVE_STAGE_PIPELINE_CHILD_ORDER) - 1),
    )
    cursor = boundary_fraction
    placements: dict[str, dict[str, Any]] = {}
    pipeline_center_y = pipeline_top + pipeline_height / 2
    for name in FIVE_STAGE_PIPELINE_CHILD_ORDER:
        child = child_by_name[name]
        width = widths[name]
        child_height = (
            width
            * child_aspects[child.get_id()]
            / max(parent_aspect, 0.01)
        )
        left = cursor + stub_ratio * width
        placements[name] = {
            "rel_pos": [
                left,
                pipeline_center_y - child_height / 2,
            ],
            "rel_width": width,
        }
        cursor += width * outer_width_factor + gap

    coordinator = child_by_name["COORDINATOR"]
    coordinator_width = min(
        available_width * 0.72 / outer_width_factor,
        coordinator_height
        * parent_aspect
        / max(child_aspects[coordinator.get_id()], 0.01),
    )
    coordinator_height_fraction = (
        coordinator_width
        * child_aspects[coordinator.get_id()]
        / max(parent_aspect, 0.01)
    )
    placements["COORDINATOR"] = {
        "rel_pos": [
            0.5 - coordinator_width / 2,
            coordinator_top
            + max(
                0.0,
                (coordinator_height - coordinator_height_fraction) / 2,
            ),
        ],
        "rel_width": coordinator_width,
    }
    return placements


def _rv32i_program_root_layout(
    children: list[Any],
    parent_aspect: float,
    title_fraction: float,
    child_aspects: dict[str, float],
    boundary_fraction: float,
    pin_size_fraction: float,
) -> dict[str, dict[str, Any]] | None:
    """Lay out the common RV32I testbench as memory - CPU - memory.

    CLOCK occupies the lower-left service area.  The placement is computed
    from each child's aspect ratio, so the tall single-cycle core and wide
    five-stage core share the same outside organization without distortion.
    """
    child_by_name = {child.get_name(): child for child in children}
    required = {
        "CLOCK",
        "CORE",
        "INSTRUCTION_MEMORY",
        "DATA_MEMORY",
    }
    if set(child_by_name) != required:
        return None

    content_top = title_fraction
    content_height = max(0.1, 1.0 - content_top)

    def fitted_width(name: str, preferred: float, height_budget: float) -> float:
        child = child_by_name[name]
        return min(
            preferred,
            height_budget
            * parent_aspect
            / max(child_aspects[child.get_id()], 0.01),
        )

    widths = {
        "INSTRUCTION_MEMORY": fitted_width(
            "INSTRUCTION_MEMORY", 0.17, content_height * 0.48
        ),
        "CORE": fitted_width("CORE", 0.40, content_height * 0.78),
        "DATA_MEMORY": fitted_width(
            "DATA_MEMORY", 0.17, content_height * 0.48
        ),
        "CLOCK": fitted_width("CLOCK", 0.13, content_height * 0.16),
    }
    preferred_lefts = {
        "INSTRUCTION_MEMORY": 0.075,
        "CORE": 0.5 - widths["CORE"] / 2,
        "DATA_MEMORY": 0.925 - widths["DATA_MEMORY"],
        "CLOCK": 0.105,
    }
    center_y = content_top + content_height * 0.43
    placements: dict[str, dict[str, Any]] = {}
    for name in ("INSTRUCTION_MEMORY", "CORE", "DATA_MEMORY"):
        child = child_by_name[name]
        rel_x, rel_width = _constrain_child_horizontal(
            preferred_lefts[name],
            widths[name],
            boundary_fraction,
            pin_size_fraction,
        )
        height = (
            rel_width
            * child_aspects[child.get_id()]
            / max(parent_aspect, 0.01)
        )
        placements[name] = {
            "rel_pos": [rel_x, center_y - height / 2],
            "rel_width": rel_width,
        }

    clock = child_by_name["CLOCK"]
    clock_x, clock_width = _constrain_child_horizontal(
        preferred_lefts["CLOCK"],
        widths["CLOCK"],
        boundary_fraction,
        pin_size_fraction,
    )
    clock_height = (
        clock_width
        * child_aspects[clock.get_id()]
        / max(parent_aspect, 0.01)
    )
    placements["CLOCK"] = {
        "rel_pos": [
            clock_x,
            content_top + content_height * 0.80 - clock_height / 2,
        ],
        "rel_width": clock_width,
    }
    return placements


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

    if _component_type(parent) == "RV32IProgramRoot":
        program_layout = _rv32i_program_root_layout(
            children,
            parent_aspect,
            title_fraction,
            child_aspects,
            boundary_fraction,
            pin_size_fraction,
        )
        if program_layout is not None:
            return program_layout

    if _component_type(parent) == "RV32IFiveStageCore":
        pipeline_layout = _five_stage_pipeline_layout(
            children,
            parent_aspect,
            title_fraction,
            child_aspects,
            boundary_fraction,
            pin_size_fraction,
        )
        if pipeline_layout is not None:
            return pipeline_layout

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

    # Some test-fixture roots do not expose output pins, so output_drivers
    # cannot identify the natural right-facing end of a feedback group. For a
    # group of at least three children, a unique node connected to more peers
    # is a conservative fallback hub (for example, one CPU core connected to
    # separate instruction and data memories). Treat hub-to-peer edges as
    # return paths so the peers and hub occupy separate columns. Two-node
    # latches and rings are intentionally left unchanged.
    cyclic_neighbors: dict[int, dict[str, set[str]]] = defaultdict(
        lambda: defaultdict(set)
    )
    for src, dst, _sink_name in edges:
        group = cyclic_group_by_node.get(src)
        if group is None or group != cyclic_group_by_node.get(dst):
            continue
        cyclic_neighbors[group][src].add(dst)
        cyclic_neighbors[group][dst].add(src)

    fallback_hub_by_group: dict[int, str] = {}
    for group_index, group in enumerate(full_sccs):
        if len(group) < 3 or any(node in output_drivers for node in group):
            continue
        degrees = {
            node: len(cyclic_neighbors[group_index].get(node, set()))
            for node in group
        }
        maximum_degree = max(degrees.values(), default=0)
        candidates = [
            node for node, degree in degrees.items()
            if degree == maximum_degree
        ]
        if maximum_degree >= 2 and len(candidates) == 1:
            fallback_hub_by_group[group_index] = candidates[0]

    active_edges: list[tuple[str, str, str]] = []
    for edge in edges:
        src, dst, sink_name = edge
        cycle_group = cyclic_group_by_node.get(src)
        same_cycle = (
            cycle_group is not None
            and cycle_group == cyclic_group_by_node.get(dst)
        )
        src_type = _component_type(child_by_id[src])
        dst_type = _component_type(child_by_id[dst])
        if same_cycle and dst_type in SEQUENTIAL_TYPES and sink_name in SEQUENTIAL_INPUT_PIN_NAMES:
            continue
        # A child that drives a parent output is the output-facing end of this
        # hierarchy.  When it also participates in a feedback cycle, orient
        # the diagram toward that child by treating its edges back into the
        # cycle as return paths.  Without this rule, a CPU core and its
        # bidirectional memories collapse into one narrow vertical column even
        # though memory responses naturally flow toward the output-facing
        # core.  This affects placement only; every circuit wire is still
        # rendered and simulated.
        if (
            same_cycle
            and src_type not in SEQUENTIAL_TYPES
            and dst_type not in SEQUENTIAL_TYPES
            and src in output_drivers
            and dst not in output_drivers
        ):
            continue
        if (
            same_cycle
            and src_type not in SEQUENTIAL_TYPES
            and dst_type not in SEQUENTIAL_TYPES
            and fallback_hub_by_group.get(cycle_group) == src
            and fallback_hub_by_group.get(cycle_group) != dst
        ):
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
    horizontal_cell = (
        (1 - 2 * padding_x) / max(1, visual_column_count)
    )
    nominal_width = (
        0.8
        if len(children) == 1
        else horizontal_cell * LAYERED_HORIZONTAL_FILL_RATIO
    )
    column_widths: list[float] = []
    for nodes in visual_columns:
        maximum_height_per_width = max(
            child_aspects[node] / max(parent_aspect, 0.01)
            + pin_size_fraction
            for node in nodes
        )
        rel_width = min(
            nominal_width,
            (inner_height / max(1, len(nodes)))
            * LAYERED_VERTICAL_FILL_RATIO
            / max(maximum_height_per_width, 0.01),
        )
        # Preserve the vertical and horizontal fit calculated above. A
        # visible-size floor can make tall children overlap in dense columns;
        # the client already handles sub-pixel components conservatively and
        # reveals them on zoom.
        rel_width = max(
            MIN_RELATIVE_CHILD_WIDTH,
            min(0.8, rel_width),
        )
        if len(nodes) >= 10:
            rel_width = min(
                rel_width,
                LAYERED_DENSE_ROW_WIDTH_CAP,
            )
        column_widths.append(rel_width)

    stub_ratio = _stub_margin_ratio(boundary_fraction, pin_size_fraction)
    maximum_width = _max_width_with_stubs(
        boundary_fraction,
        stub_ratio,
    )
    column_widths = [
        min(width, maximum_width)
        for width in column_widths
    ]

    available_width = max(0.0, 1 - 2 * boundary_fraction)
    outer_width_factor = 1 + 2 * stub_ratio
    if visual_column_count > 1:
        # Keep a predictable portion of the parent available as wire gutters.
        # If the preferred heterogeneous column widths do not fit, scale them
        # together while retaining their useful relative sizes.
        minimum_total_gutter = (
            available_width
            * max(0.0, 1 - LAYERED_HORIZONTAL_FILL_RATIO)
        )
        body_budget = max(
            0.0,
            available_width - minimum_total_gutter,
        ) / max(outer_width_factor, 0.01)
        preferred_body_width = sum(column_widths)
        if preferred_body_width > body_budget:
            scale = body_budget / max(
                preferred_body_width,
                MIN_RELATIVE_CHILD_WIDTH,
            )
            column_widths = [
                max(MIN_RELATIVE_CHILD_WIDTH, width * scale)
                for width in column_widths
            ]

    total_outer_width = sum(
        width * outer_width_factor
        for width in column_widths
    )
    inter_column_gap = (
        max(0.0, available_width - total_outer_width)
        / max(1, visual_column_count - 1)
        if visual_column_count > 1
        else 0.0
    )

    column_centers: list[float] = []
    if visual_column_count == 1:
        column_centers.append(0.5)
    else:
        outer_cursor = boundary_fraction
        for rel_width in column_widths:
            column_centers.append(
                outer_cursor
                + stub_ratio * rel_width
                + rel_width / 2
            )
            outer_cursor += (
                rel_width * outer_width_factor
                + inter_column_gap
            )

    result: dict[str, dict[str, Any]] = {}
    for column_index, nodes in enumerate(visual_columns):
        rel_width = column_widths[column_index]
        x_center = column_centers[column_index]
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

        self.schema_version = int(data.get("schema_version", 1))
        self.settings = data.get("default_settings", _clone(DEFAULT_SETTINGS))
        self.type_layouts = data.get("type_layouts", {})
        self.profile_layouts = data.get("profile_layouts", {})
        self.root_layouts = data.get("root_layouts", {})
        self.loaded_mtime = self._current_mtime()
        self.is_dirty = False

    def _current_mtime(self) -> int | None:
        try:
            return self.filepath.stat().st_mtime_ns
        except FileNotFoundError:
            return None

    @staticmethod
    def _merge_layouts(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
        merged = _clone(base)
        merged.update(_clone(override))
        merged["children"] = {
            **_clone(base.get("children", {})),
            **_clone(override.get("children", {})),
        }
        return merged

    def get_layout_for(self, component_type: str) -> dict[str, Any]:
        return _clone(self.type_layouts.get(component_type, {}))

    def get_layout_for_component(self, component: Any) -> dict[str, Any]:
        component_type = _component_type(component)
        component_layout_type = _component_layout_type(component)
        layout = self.get_layout_for(component_type)
        if component_layout_type != component_type:
            layout = self._merge_layouts(layout, self.get_layout_for(component_layout_type))
        profile_fingerprint = _component_profile_fingerprint(component)
        if profile_fingerprint != "explicit":
            profile_layout = self.profile_layouts.get(profile_fingerprint, {}).get(component_layout_type, {})
            layout = self._merge_layouts(layout, profile_layout)
        return layout

    def get_root_layout_for(self, scenario_key: str, root_type: str) -> dict[str, Any]:
        type_layout = self.type_layouts.get(root_type, {})
        root_layout = self.root_layouts.get(scenario_key, {})
        return self._merge_layouts(type_layout, root_layout)

    def get_root_layout_for_component(self, scenario_key: str, component: Any) -> dict[str, Any]:
        type_layout = self.get_layout_for_component(component)
        root_layout = self.root_layouts.get(scenario_key, {})
        return self._merge_layouts(type_layout, root_layout)

    def get_child_layout_for_component(
        self, component: Any, child_name: str, *, scenario_key: str | None = None, is_root: bool = False
    ) -> dict[str, Any]:
        parent_layout = (
            self.get_root_layout_for_component(scenario_key, component)
            if is_root and scenario_key
            else self.get_layout_for_component(component)
        )
        return _clone(parent_layout.get("children", {}).get(child_name, {}))

    def minimum_aspect_for_component(self, component: Any) -> float:
        return _minimum_aspect_ratio_for_pins(
            component,
            self.settings,
            self.get_layout_for_component(component),
        )

    def visual_ratio_for_component(
        self,
        component: Any,
        setting_name: str,
        *,
        scenario_key: str | None = None,
        is_root: bool = False,
    ) -> float:
        layout = (
            self.get_root_layout_for_component(
                scenario_key, component
            )
            if is_root and scenario_key
            else self.get_layout_for_component(component)
        )
        return max(
            0.0,
            _float_value(
                layout.get(setting_name),
                _float_value(
                    self.settings.get(setting_name),
                    DEFAULT_SETTINGS[setting_name],
                ),
            ),
        )

    @staticmethod
    def _coerced_aspect(layout: dict[str, Any], fallback: float, minimum: float) -> float:
        if "aspect_ratio" not in layout:
            return round(minimum, 3)
        return round(max(_float_value(layout.get("aspect_ratio"), fallback), minimum), 3)

    def effective_minimum_aspect_for_component(
        self, component: Any, *, scenario_key: str | None = None, is_root: bool = False
    ) -> float:
        instance_minimum = self.minimum_aspect_for_component(component)
        layout = self.get_root_layout_for_component(scenario_key, component) if is_root and scenario_key else self.get_layout_for_component(component)
        return round(max(instance_minimum, _float_value(layout.get("min_aspect_ratio"), instance_minimum)), 3)

    def aspect_for_component(self, component: Any, *, scenario_key: str | None = None, is_root: bool = False) -> float:
        fallback = 0.75 if is_root else 1.0
        minimum = self.effective_minimum_aspect_for_component(
            component,
            scenario_key=scenario_key,
            is_root=is_root,
        )
        layout = self.get_root_layout_for_component(scenario_key, component) if is_root and scenario_key else self.get_layout_for_component(component)
        return self._coerced_aspect(layout, fallback, minimum)

    def ensure_component_layout_defaults(
        self, component: Any, *, scenario_key: str | None = None, is_root: bool = False
    ) -> None:
        component_type = _component_layout_type(component)
        layout = self.type_layouts.setdefault(component_type, {})
        for key, value in COMPONENT_LAYOUT_DEFAULTS.get(
            component_type, {}
        ).items():
            if key not in layout:
                layout[key] = value
                self.is_dirty = True
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

        if (
            not is_root
            or not scenario_key
            or _component_type(component) != "Component"
        ):
            return

        children = list(component.get_children())
        if not children:
            return
        tallest_child = max(
            self.minimum_aspect_for_component(child)
            for child in children
        )
        # A generic test wrapper has no pins of its own, so its normal minimum
        # aspect says nothing about a tall child such as a pipeline core.
        # Reserve roughly 28% of the root width and 78% of its height for that
        # child. This raises only the scenario-root aspect and does not alter
        # reusable component type layouts.
        child_driven_minimum = min(
            2.5,
            tallest_child * (0.28 / 0.78),
        )
        root_layout = self.root_layouts.setdefault(
            scenario_key, {}
        )
        root_minimum = round(
            max(
                instance_minimum,
                child_driven_minimum,
                _float_value(
                    root_layout.get("min_aspect_ratio"),
                    instance_minimum,
                ),
            ),
            3,
        )
        root_aspect = self._coerced_aspect(
            root_layout, 0.75, root_minimum
        )
        if root_layout.get("min_aspect_ratio") != root_minimum:
            root_layout["min_aspect_ratio"] = root_minimum
            self.is_dirty = True
        if root_layout.get("aspect_ratio") != root_aspect:
            root_layout["aspect_ratio"] = root_aspect
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

    def update_profile_child_layout(
        self,
        profile_fingerprint: str,
        parent_type: str,
        child_name: str,
        rel_pos: list[float],
        rel_width: float,
    ) -> None:
        profile = self.profile_layouts.setdefault(profile_fingerprint, {})
        type_layout = profile.setdefault(parent_type, {})
        child_entry = type_layout.setdefault("children", {}).setdefault(child_name, {})
        if child_entry.get("rel_pos") == rel_pos and child_entry.get("rel_width") == rel_width:
            return
        child_entry["rel_pos"] = rel_pos
        child_entry["rel_width"] = rel_width
        self.is_dirty = True

    def set_child_layouts(
        self,
        component: Any,
        placements: dict[str, dict[str, Any]],
        *,
        scenario_key: str | None = None,
        is_root: bool = False,
    ) -> None:
        parent_type = _component_layout_type(component)
        profile_fingerprint = _component_profile_fingerprint(component)
        for child_name, placement in placements.items():
            rel_pos = placement["rel_pos"]
            rel_width = placement["rel_width"]
            if scenario_key and is_root:
                self.update_root_child_layout(scenario_key, child_name, rel_pos, rel_width)
            elif profile_fingerprint != "explicit":
                self.update_profile_child_layout(
                    profile_fingerprint,
                    parent_type,
                    child_name,
                    rel_pos,
                    rel_width,
                )
            else:
                self.update_type_child_layout(parent_type, child_name, rel_pos, rel_width)

    def to_jsonable(self) -> dict[str, Any]:
        return {
            "schema_version": LAYOUT_SCHEMA_VERSION,
            "default_settings": self.settings,
            "type_layouts": self.type_layouts,
            "profile_layouts": self.profile_layouts,
            "root_layouts": self.root_layouts,
        }

    def replace_from_client(self, layout: dict[str, Any]) -> None:
        self.schema_version = int(layout.get("schema_version", LAYOUT_SCHEMA_VERSION))
        self.settings = _clone(layout.get("default_settings", {}))
        self.type_layouts = _clone(layout.get("type_layouts", {}))
        self.profile_layouts = _clone(layout.get("profile_layouts", {}))
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


def scenario_layout_key(scenario_key: str, root: Any) -> str:
    profile_fingerprint = _component_profile_fingerprint(root)
    if profile_fingerprint == "explicit":
        return scenario_key
    return f"{scenario_key}@{profile_fingerprint}"


def visualizable_test_names() -> list[str]:
    return [
        descriptor["name"]
        for descriptor
        in circuit_backend.get_registered_test_descriptors()
        if descriptor["visualizable"]
        and descriptor["name"] not in NON_VISUALIZABLE_TESTS
    ]


def _scenario_classes() -> dict[str, Any]:
    # Expose registry tests that build a reusable topology during setup_circuit().
    # The excluded tests intentionally verify API/validation behavior without a
    # meaningful visual root.

    scenarios: dict[str, Any] = {}
    for class_name in visualizable_test_names():
        def scenario_factory(
            registered_name: str = class_name,
        ) -> Any:
            return circuit_backend.create_test_by_name(
                registered_name
            )

        aliases = {scenario_default_alias(class_name), class_name.lower(), *PREFERRED_SCENARIO_ALIASES.get(class_name, [])}
        for alias in aliases:
            scenarios[alias] = scenario_factory
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
    def __init__(
        self,
        scenario_key: str,
        layout_manager: LayoutManager,
        *,
        profile_revision: int = 0,
        profile_overrides: dict[str, str] | None = None,
    ):
        self.scenario_key = normalize_scenario(scenario_key)
        self.layout_manager = layout_manager
        self.lock = threading.RLock()
        self.session_id = uuid.uuid4().hex
        self.profile_revision = profile_revision
        self.profile_overrides = validate_overrides(
            profile_overrides or {}
        )

        self.test_scenario = SCENARIOS[self.scenario_key]()
        self.profile_editable = bool(
            self.test_scenario.supports_build_profile()
        )
        if self.profile_overrides:
            if not self.profile_editable:
                raise ValueError(
                    f"Scenario '{self.scenario_key}' does not accept "
                    "component fidelity changes"
                )
            base_profile = self.test_scenario.get_build_profile()
            profile = circuit_backend.profile_with_exact_overrides(
                base_profile,
                self.profile_overrides,
                f"visualizer-{self.scenario_key}",
            )
            self.test_scenario.set_build_profile(profile)
        self.test_scenario.setup_circuit()
        self.root = self.test_scenario.get_root()
        if not self.root:
            raise RuntimeError("C++ test scenario did not produce a root component")
        self.profile_fingerprint = _component_profile_fingerprint(self.root)
        self.layout_key = scenario_layout_key(self.scenario_key, self.root)

        self.simulator = self.test_scenario.get_simulator()
        if not self.test_scenario.is_simulation_precomputed():
            self.test_scenario.schedule_initial_events(0)
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
        self.performance_metrics = [
            {
                "name": metric.name,
                "value": metric.value,
                "unit": metric.unit,
                "description": metric.description,
            }
            for metric in self.test_scenario.get_performance_metrics()
        ]
        self.simulator.set_circuit_state_at_time(self.timestamps[0])

        self.components: list[dict[str, Any]] = []
        self.pins: list[dict[str, Any]] = []
        self.wires: list[dict[str, Any]] = []
        self.pin_records_by_component: dict[
            str, list[dict[str, Any]]
        ] = defaultdict(list)
        self.wire_records_by_owner: dict[
            str, list[dict[str, Any]]
        ] = defaultdict(list)
        self.component_handles: dict[str, Any] = {}
        self.component_depths: dict[str, int] = {}
        self.pin_handles: dict[str, Any] = {}
        self.wire_handles: dict[str, Any] = {}
        self.pin_handles_by_index: list[Any] = []
        self.wire_handles_by_index: list[Any] = []
        self.pin_handles_by_component_name: dict[tuple[str, str], Any] = {}
        self._build_topology()
        self._validate_topology_endpoints()
        self.signal_snapshot = circuit_backend.VisualSignalSnapshot(
            self.pin_handles_by_index,
            self.wire_handles_by_index,
        )
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
            scenario_key=self.layout_key,
            is_root=is_root,
        )
        if is_root:
            layout = self.layout_manager.get_root_layout_for_component(self.layout_key, component)
        else:
            layout = self.layout_manager.get_layout_for_component(component)
        return aspect_ratio, layout.get("color", DEFAULT_COLOR)

    def _build_topology(self) -> None:
        def walk(
            component: Any,
            parent_index: int | None,
            depth: int,
        ) -> None:
            component_id = component.get_id()
            component_type = component.get_type_name()
            component_layout_type = _component_layout_type(component)
            children = list(component.get_children())
            self.layout_manager.ensure_component_layout_defaults(
                component,
                scenario_key=self.layout_key,
                is_root=depth == 0,
            )
            aspect_ratio, color = self._component_layout_meta(component)
            self.component_handles[component_id] = component
            self.component_depths[component_id] = depth

            component_data = {
                "id": component_id,
                "name": component.get_name(),
                "type": component_type,
                "parentIndex": parent_index,
                "depth": depth,
                "aspectRatio": aspect_ratio,
                "minAspectRatio": self.layout_manager.effective_minimum_aspect_for_component(
                    component,
                    scenario_key=self.layout_key,
                    is_root=depth == 0,
                ),
                "color": color,
                "implementationId": component.get_implementation_id(),
                "fidelity": component.get_selected_fidelity(),
            }
            if component_layout_type != component_type:
                component_data["layoutType"] = component_layout_type
            available_fidelities = component.get_available_fidelities()
            if available_fidelities:
                component_data["availableFidelities"] = (
                    available_fidelities
                )
            if component.is_profile_selectable():
                component_data["profileSelectable"] = True
            profile_fingerprint = component.get_profile_fingerprint()
            if profile_fingerprint != "explicit":
                component_data["profileFingerprint"] = (
                    profile_fingerprint
                )
            if component_type == "ConstantValue":
                try:
                    component_data["constantValue"] = int(component.get_constant_value())
                except (AttributeError, RuntimeError, TypeError, ValueError):
                    pass
            if component_type == "Rewire":
                try:
                    unmapped_default = str(component.get_unmapped_default()).split(".")[-1].upper()
                    component_data["rewire"] = {
                        "inputs": [
                            {"name": str(spec.name), "width": int(spec.width)}
                            for spec in component.get_input_specs()
                        ],
                        "outputs": [
                            {"name": str(spec.name), "width": int(spec.width)}
                            for spec in component.get_output_specs()
                        ],
                        "mappings": [
                            {
                                "srcWire": str(mapping.src_wire),
                                "srcBit": int(mapping.src_bit),
                                "dstWire": str(mapping.dst_wire),
                                "dstBit": int(mapping.dst_bit),
                            }
                            for mapping in component.get_bit_mappings()
                        ],
                        "unmappedDefault": unmapped_default,
                    }
                except (AttributeError, RuntimeError, TypeError, ValueError):
                    pass
            component_index = len(self.components)
            self.components.append(component_data)

            if hasattr(component, "get_input_pins"):
                input_pins = sorted(
                    component.get_input_pins().items(),
                    key=lambda item: _natural_key(item[0]),
                )
                output_pins = sorted(
                    component.get_output_pins().items(),
                    key=lambda item: _natural_key(item[0]),
                )
                for pin_name, pin in input_pins:
                    self._add_pin(component_id, pin_name, pin, "input")
                for pin_name, pin in output_pins:
                    self._add_pin(component_id, pin_name, pin, "output")

            self._add_wires(component)
            self._ensure_child_defaults(component, children, depth)
            for child in children:
                walk(child, component_index, depth + 1)

        walk(self.root, None, 0)

    def _validate_topology_endpoints(self) -> None:
        missing: list[str] = []
        for wire in self.wires:
            source_pin_id = wire["sourcePinId"]
            if source_pin_id is not None and source_pin_id not in self.pin_handles:
                missing.append(f'{wire["id"]}: source {source_pin_id}')
            for sink_pin_id in wire["sinkPinIds"]:
                if sink_pin_id not in self.pin_handles:
                    missing.append(f'{wire["id"]}: sink {sink_pin_id}')

        if missing:
            preview = "; ".join(missing[:5])
            remainder = len(missing) - 5
            suffix = f"; and {remainder} more" if remainder > 0 else ""
            raise RuntimeError(
                "Visualizer topology contains wire endpoints whose component types are not "
                f"fully exposed to Python ({preview}{suffix})."
            )

    def _add_pin(self, component_id: str, pin_name: str, pin: Any, pin_type: str) -> None:
        try:
            pin_id = pin.get_id()
        except AttributeError:
            pin_id = f"{component_id}.{pin_name}"
        state_index = len(self.pin_handles_by_index)
        self.pin_handles[pin_id] = pin
        self.pin_handles_by_index.append(pin)
        self.pin_handles_by_component_name[(component_id, pin_name)] = pin
        record = {
            "id": pin_id,
            "componentId": component_id,
            "name": pin_name,
            "type": pin_type,
            "width": _signal_width(pin),
            "stateIndex": state_index,
        }
        self.pins.append(record)
        self.pin_records_by_component[component_id].append(record)

    def _add_wires(self, component: Any) -> None:
        for wire in component.get_wires():
            wire_id = wire.get_id()
            state_index = len(self.wire_handles_by_index)
            self.wire_handles[wire_id] = wire
            self.wire_handles_by_index.append(wire)
            source_pin = wire.get_source_pin()
            source_pin_id = source_pin.get_id() if source_pin else None
            sink_pin_ids = [pin.get_id() for pin in wire.get_sink_pins()]
            record = {
                "id": wire_id,
                "name": wire.get_name(),
                "ownerId": component.get_id(),
                "sourcePinId": source_pin_id,
                "sinkPinIds": sink_pin_ids,
                "width": _signal_width(wire),
                "stateIndex": state_index,
            }
            self.wires.append(record)
            self.wire_records_by_owner[
                component.get_id()
            ].append(record)

    def topology_slice(
        self,
        owner_ids: list[str],
    ) -> dict[str, Any]:
        if not isinstance(owner_ids, list) or not owner_ids:
            raise ValueError(
                "Topology scope request requires at least one owner id"
            )
        if len(owner_ids) > 512:
            raise ValueError(
                "Topology scope request may contain at most 512 owners"
            )

        scope_ids: list[str] = []
        pin_component_ids: set[str] = set()
        seen_scopes: set[str] = set()
        for owner_id in owner_ids:
            if not isinstance(owner_id, str):
                raise ValueError(
                    "Topology scope owner ids must be strings"
                )
            if owner_id in seen_scopes:
                continue
            owner = self.component_handles.get(owner_id)
            if owner is None:
                raise ValueError(
                    f"Unknown topology scope owner '{owner_id}'"
                )
            seen_scopes.add(owner_id)
            scope_ids.append(owner_id)
            pin_component_ids.add(owner_id)
            pin_component_ids.update(
                child.get_id()
                for child in owner.get_children()
            )

        pins = [
            record
            for component_id in pin_component_ids
            for record in self.pin_records_by_component.get(
                component_id, ()
            )
        ]
        wires = [
            record
            for owner_id in scope_ids
            for record in self.wire_records_by_owner.get(owner_id, ())
        ]
        return {
            "sessionId": self.session_id,
            "scopeIds": scope_ids,
            "pins": pins,
            "wires": wires,
        }

    def _default_child_layouts(
        self,
        component: Any,
        children: list[Any],
        depth: int,
        layout_manager: LayoutManager | None = None,
    ) -> dict[str, dict[str, Any]]:
        manager = layout_manager or self.layout_manager
        is_root = depth == 0
        parent_aspect = manager.aspect_for_component(component, scenario_key=self.layout_key, is_root=is_root)
        title_ratio = manager.visual_ratio_for_component(
            component,
            "title_bar_ratio",
            scenario_key=self.layout_key,
            is_root=is_root,
        )
        title_fraction = min(0.7, title_ratio / max(parent_aspect, 0.01))
        boundary_fraction = manager.visual_ratio_for_component(
            component,
            "boundary_area_ratio",
            scenario_key=self.layout_key,
            is_root=is_root,
        )
        pin_size_fraction = manager.visual_ratio_for_component(
            component,
            "pin_size_ratio",
            scenario_key=self.layout_key,
            is_root=is_root,
        )
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

        parent_type = _component_layout_type(component)
        is_root = depth == 0
        missing_children = [
            child
            for child in children
            if not (
                "rel_pos" in self.layout_manager.get_child_layout_for_component(
                    component,
                    child.get_name(),
                    scenario_key=self.layout_key,
                    is_root=is_root,
                )
                and "rel_width" in self.layout_manager.get_child_layout_for_component(
                    component,
                    child.get_name(),
                    scenario_key=self.layout_key,
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
            component,
            missing_placements,
            scenario_key=self.layout_key,
            is_root=is_root,
        )

        for child in children:
            child_layout = self.layout_manager.get_child_layout_for_component(
                component,
                child.get_name(),
                scenario_key=self.layout_key,
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
            "layoutKey": self.layout_key,
            "parentId": parent_id,
            "parentType": parent.get_type_name(),
            "parentLayoutType": _component_layout_type(parent),
            "isRoot": depth == 0,
            "placements": placements,
        }

    def topology_response(self) -> dict[str, Any]:
        initial_topology = self.topology_slice(
            [self.root.get_id()]
        )
        return {
            "sessionId": self.session_id,
            "scenario": self.scenario_key,
            "layoutKey": self.layout_key,
            "profileFingerprint": self.profile_fingerprint,
            "rootId": self.root.get_id(),
            "rootType": self.root.get_type_name(),
            "topologyEncoding": "progressive-v1",
            "components": self.components,
            "pins": initial_topology["pins"],
            "wires": initial_topology["wires"],
            "loadedScopeIds": initial_topology["scopeIds"],
            "timestamps": self.timestamps,
            "checkpoints": self.checkpoints,
            "performanceMetrics": self.performance_metrics,
            "layout": self.layout_manager.to_jsonable(),
            "state": self.state_at_index(0),
            "stateEncoding": "indexed-v1",
            "profile": {
                "editable": self.profile_editable,
                "revision": self.profile_revision,
                "exactOverrides": self.profile_overrides,
                "fingerprint": self.profile_fingerprint,
            },
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
            pins, wires = self.signal_snapshot.get_values()
            return {
                "encoding": "indexed-v1",
                "index": index,
                "time": timestamp,
                "pins": pins,
                "wires": wires,
            }

    def _component_pin_value(self, component_id: str, pin_name: str) -> str | None:
        handle = self.pin_handles_by_component_name.get((component_id, pin_name))
        return _signal_value(handle) if handle else None

    def _component_ports(self, component_id: str, pin_names: tuple[str, ...]) -> dict[str, dict[str, Any]]:
        ports: dict[str, dict[str, Any]] = {}
        for pin_name in pin_names:
            value = self._component_pin_value(component_id, pin_name)
            if value is None:
                continue
            ports[pin_name] = {
                "value": value,
                "hex": _binary_value_to_hex(value),
            }
        return ports

    def _register_file_state(self, component: Any, component_id: str, index: int, timestamp: int) -> dict[str, Any]:
        register_words = circuit_backend.get_register_state_at_time(
            component,
            timestamp,
        )
        registers: list[dict[str, Any]] = []
        for register_index, word in enumerate(register_words):
            bits = _logic_vector_value(word)
            hex_value = _binary_value_to_hex(bits)
            registers.append(
                {
                    "index": register_index,
                    "name": f"x{register_index}",
                    "bits": bits,
                    "hex": hex_value,
                    "known": hex_value is not None,
                }
            )

        ports = self._component_ports(
            component_id,
            (
                "RS1_ADDR",
                "RS2_ADDR",
                "RD_ADDR",
                "WRITE_DATA",
                "REG_WRITE",
                "CLK",
                "RST",
                "RS1_DATA",
                "RS2_DATA",
            ),
        )
        return {
            "index": index,
            "time": timestamp,
            "componentId": component_id,
            "type": "RegisterFile32x32",
            "supported": True,
            "registers": registers,
            "ports": ports,
        }

    def _memory64kx32_state(self, component: Any, component_id: str, index: int, timestamp: int) -> dict[str, Any]:
        ports = self._component_ports(
            component_id,
            (
                "ADDR",
                "WRITE_DATA",
                "READ_EN",
                "WRITE_EN",
                "SIZE",
                "SIGN_EXTEND",
                "CLK",
                "RST",
                "READ_DATA",
                "READY",
                "FAULT",
            ),
        )
        active_address = _binary_value_to_int(ports.get("ADDR", {}).get("value"))
        active_base = active_address & ~0x3 if active_address is not None else None
        read_active = ports.get("READ_EN", {}).get("value") == "1"
        write_active = ports.get("WRITE_EN", {}).get("value") == "1"
        memory_role = "data" if component_id.endswith(".DATA_MEMORY") or component_id.endswith("DATA_MEMORY") else "generic"
        if (
            self.scenario_key == "rv32i-program-loader"
            or component_id.endswith(".INSTRUCTION_MEMORY")
            or component_id.endswith("INSTRUCTION_MEMORY")
        ):
            memory_role = "instruction"
        window_active = active_base is not None and (read_active or write_active)
        if window_active:
            window_words = component.get_words_at_time(timestamp, active_base, 1)
        else:
            window_words = []
        touched_words = component.get_touched_words_at_time(timestamp, 64 * 1024)

        return {
            "index": index,
            "time": timestamp,
            "componentId": component_id,
            "type": "Memory64Kx32",
            "supported": True,
            "capacityBytes": 64 * 1024 * 4,
            "capacityWords": 64 * 1024,
            "memoryRole": memory_role,
            "windowActive": window_active,
            "activeAddress": active_address,
            "activeBaseAddress": active_base,
            "touchedWordCount": len(touched_words),
            "touchedWords": [
                _memory_word_entry(address, word, decode_instruction=memory_role == "instruction")
                for address, word in touched_words
            ],
            "windowWords": [
                _memory_word_entry(address, word, decode_instruction=memory_role == "instruction")
                for address, word in window_words
            ],
            "ports": ports,
        }

    def component_state_at_index(self, index: int, component_id: str) -> dict[str, Any]:
        with self.lock:
            component = self.component_handles.get(component_id)
            if component is None:
                raise ValueError(f"Unknown component id '{component_id}'")

            index = max(0, min(index, len(self.timestamps) - 1))
            timestamp = int(self.timestamps[index])
            self.simulator.set_circuit_state_at_time(timestamp)
            component_type = _component_type(component)
            if component_type == "RegisterFile32x32":
                return self._register_file_state(component, component_id, index, timestamp)
            if component_type == "Memory64Kx32":
                return self._memory64kx32_state(component, component_id, index, timestamp)
            return {
                "index": index,
                "time": timestamp,
                "componentId": component_id,
                "type": component_type,
                "supported": False,
            }


class SessionStore:
    def __init__(self) -> None:
        self.layout_manager = LayoutManager(LAYOUT_PATH)
        self.profile_store = ProfileStore(PROFILE_PATH)
        try:
            configured_limit = int(os.environ.get("VISUALIZER_V2_MAX_SESSIONS", "2"))
        except ValueError:
            configured_limit = 2
        self.max_sessions = max(1, configured_limit)
        self.sessions: OrderedDict[str, CircuitSession] = OrderedDict()
        self.active_sessions: dict[str, str] = {}
        self.lock = threading.RLock()

    def _reload_layout_if_changed(self) -> None:
        current_mtime = self.layout_manager._current_mtime()
        if current_mtime != self.layout_manager.loaded_mtime:
            self.layout_manager = LayoutManager(LAYOUT_PATH)
            self.sessions.clear()
            self.active_sessions.clear()

    def _remember(self, session: CircuitSession) -> CircuitSession:
        self.sessions[session.session_id] = session
        self.active_sessions[session.scenario_key] = session.session_id
        self.sessions.move_to_end(session.session_id)
        while len(self.sessions) > self.max_sessions:
            expired_id, _ = self.sessions.popitem(last=False)
            for scenario, session_id in list(
                self.active_sessions.items()
            ):
                if session_id == expired_id:
                    del self.active_sessions[scenario]
        return session

    def _new_for_saved_profile(
        self,
        scenario_key: str,
    ) -> CircuitSession:
        revision, overrides = self.profile_store.get(scenario_key)
        return self._remember(CircuitSession(
            scenario_key,
            self.layout_manager,
            profile_revision=revision,
            profile_overrides=overrides,
        ))

    def get(self, scenario: str) -> CircuitSession:
        scenario_key = normalize_scenario(scenario)
        with self.lock:
            self._reload_layout_if_changed()
            session_id = self.active_sessions.get(scenario_key)
            session = self.sessions.get(session_id or "")
            revision, overrides = self.profile_store.get(scenario_key)
            if (
                session is None
                or session.profile_revision != revision
                or session.profile_overrides != overrides
            ):
                return self._new_for_saved_profile(scenario_key)
            self.sessions.move_to_end(session.session_id)
            return session

    def get_by_id(self, session_id: str) -> CircuitSession:
        with self.lock:
            session = self.sessions.get(session_id)
            if session is None:
                raise ValueError(
                    "Simulation session expired; apply or reload the scenario"
                )
            self.sessions.move_to_end(session_id)
            return session

    def apply_profile(
        self,
        scenario: str,
        expected_revision: int,
        overrides: dict[str, str],
    ) -> CircuitSession:
        scenario_key = normalize_scenario(scenario)
        normalized = validate_overrides(overrides)
        with self.lock:
            self._reload_layout_if_changed()
            current = self.get(scenario_key)
            current_revision, saved = self.profile_store.get(
                scenario_key
            )
            if expected_revision != current_revision:
                raise ValueError(
                    "Profile changed since it was loaded; reload the "
                    "scenario before applying again"
                )
            if not current.profile_editable:
                raise ValueError(
                    f"Scenario '{scenario_key}' does not accept "
                    "component fidelity changes"
                )

            for path, fidelity in normalized.items():
                if saved.get(path) == fidelity:
                    continue
                component = current.component_handles.get(path)
                if component is None:
                    raise ValueError(
                        f"Cannot add an override for unknown component "
                        f"'{path}'"
                    )
                available = component.get_available_fidelities()
                if fidelity not in available:
                    raise ValueError(
                        f"Component '{path}' does not provide "
                        f"{fidelity} fidelity"
                    )

            candidate = CircuitSession(
                scenario_key,
                self.layout_manager,
                profile_revision=current_revision + 1,
                profile_overrides=normalized,
            )
            next_revision = self.profile_store.replace(
                scenario_key,
                current_revision,
                normalized,
            )
            candidate.profile_revision = next_revision
            return self._remember(candidate)

    def replace_layout(self, layout: dict[str, Any]) -> None:
        with self.lock:
            self.layout_manager.replace_from_client(layout)
            self.layout_manager.save()

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
            session = self.get(scenario_key)
            return session.default_child_layouts(
                parent_id, child_name, layout)


STORE = SessionStore()


def normalize_scenario(scenario: str | None) -> str:
    key = (scenario or DEFAULT_SCENARIO).strip().lower().replace("_", "-")
    if key not in SCENARIOS:
        valid = ", ".join(sorted(SCENARIOS))
        raise ValueError(f"Unknown scenario '{scenario}'. Valid scenarios: {valid}")
    return SCENARIO_ALIASES_TO_CANONICAL.get(key, key)


def json_response(handler: SimpleHTTPRequestHandler, payload: Any, status: int = 200) -> None:
    encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    content_encoding: str | None = None
    accepted_encodings = handler.headers.get("Accept-Encoding", "").lower()
    if len(encoded) >= 1024 and "gzip" in accepted_encodings:
        compressed = gzip.compress(encoded, compresslevel=1)
        if len(compressed) < len(encoded):
            encoded = compressed
            content_encoding = "gzip"
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(encoded)))
    handler.send_header("Cache-Control", "no-store")
    handler.send_header("Vary", "Accept-Encoding")
    if content_encoding:
        handler.send_header("Content-Encoding", content_encoding)
    handler.end_headers()
    try:
        handler.wfile.write(encoded)
    except (BrokenPipeError, ConnectionResetError):
        # Browsers commonly cancel an obsolete large-circuit request when
        # navigating to another scenario. The session remains valid and there
        # is no useful error response to send after the peer has disconnected.
        return


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
                index = int(query.get("index", [0])[0])
                session_id = query.get("session", [""])[0]
                session = (
                    STORE.get_by_id(session_id)
                    if session_id
                    else STORE.get(
                        query.get(
                            "scenario",
                            [DEFAULT_SCENARIO],
                        )[0])
                )
                json_response(self, session.state_at_index(index))
            elif path == "/api/component-state":
                index = int(query.get("index", [0])[0])
                component_id = query.get("componentId", [""])[0]
                if not component_id:
                    raise ValueError("GET /api/component-state requires componentId")
                session_id = query.get("session", [""])[0]
                session = (
                    STORE.get_by_id(session_id)
                    if session_id
                    else STORE.get(
                        query.get(
                            "scenario",
                            [DEFAULT_SCENARIO],
                        )[0])
                )
                json_response(
                    self,
                    session.component_state_at_index(
                        index, component_id))
            else:
                self._serve_static(path, raw_path)
        except Exception as exc:
            error_response(self, str(exc), 500 if not isinstance(exc, ValueError) else 400)

    def do_POST(self) -> None:  # noqa: N802 - http.server API
        parsed = urlparse(self.path)
        path = self._normalize_path(parsed.path)
        if path not in {
            "/api/layout",
            "/api/layout/defaults",
            "/api/circuit/apply",
            "/api/topology",
        }:
            error_response(self, "Unknown POST endpoint", 404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length).decode("utf-8")
            payload = json.loads(body or "{}")
            if path == "/api/topology":
                session_id = payload.get("session")
                owner_ids = payload.get("ownerIds")
                if not isinstance(session_id, str) or not session_id:
                    raise ValueError(
                        "POST /api/topology requires a session id"
                    )
                if not isinstance(owner_ids, list):
                    raise ValueError(
                        "POST /api/topology requires an ownerIds array"
                    )
                json_response(
                    self,
                    STORE.get_by_id(session_id).topology_slice(
                        owner_ids
                    ),
                )
                return
            if path == "/api/circuit/apply":
                scenario = payload.get("scenario")
                revision = payload.get("revision")
                overrides = payload.get("exactOverrides")
                if not isinstance(scenario, str):
                    raise ValueError(
                        "POST /api/circuit/apply requires scenario")
                if not isinstance(revision, int) or revision < 0:
                    raise ValueError(
                        "POST /api/circuit/apply requires a "
                        "non-negative revision")
                session = STORE.apply_profile(
                    scenario,
                    revision,
                    validate_overrides(overrides),
                )
                json_response(
                    self,
                    session.topology_response())
                return
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
    print(f"Profile file: {PROFILE_PATH}")
    server.serve_forever()


if __name__ == "__main__":
    main()
